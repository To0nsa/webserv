/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   HttpRequestParser.cpp                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: Invalid date        by                   #+#    #+#             */
/*   Updated: 2025/08/19 10:00:45 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    HttpRequestParser.cpp
 * @brief   Implements the HttpRequestParser class and HTTP request parsing logic.
 *
 * @details This file defines the parsing pipeline that converts raw client
 *          input into structured @ref HttpRequest objects:
 *
 *          - **Request line parsing**: method, request-target, and HTTP version.
 *          - **Header validation**: validates and inserts headers, handling
 *            conflicts between `Content-Length` and `Transfer-Encoding`.
 *          - **Path normalization**: decodes percent-encoding, strips fragments,
 *            validates against traversal attempts, and normalizes paths.
 *          - **Server matching**: selects the most appropriate @ref Server
 *            configuration from the available virtual hosts, based on the Host header.
 *          - **Body parsing**: supports both fixed-length bodies (via
 *            `Content-Length`) and chunked transfer encoding, enforcing
 *            `client_max_body_size` limits.
 *          - **Validation**: ensures method support, mandatory headers,
 *            and content type restrictions for POST requests.
 *
 *          Utility functions inside anonymous namespaces support RFC-compliant
 *          token validation, path checking, URL parsing, and body handling.
 *
 *          The main entry point is @ref HttpRequestParser::parse, which integrates
 *          all steps to safely parse complete requests, handle errors, and detect
 *          pipelined requests.
 *
 * @ingroup http
 */

#include "http/HttpRequestParser.hpp"
#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/Url.hpp"              // for Url
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for normalizePath
#include "utils/stringUtils.hpp"     // for toLower, toUpper, trim
#include "utils/urlUtils.hpp"        // for decodePercentEncoding
#include <algorithm>                 // for all_of, any_of, transform
#include <array>                     // for array
#include <cctype>                    // for isalnum, iscntrl, isdigit, tolower
#include <exception>                 // for exception
#include <filesystem>                // for path
#include <regex>                     // for match_results, regex_match, regex
#include <set>                       // for set, operator==, _Rb_tree_const...
#include <sstream>                   // for basic_istream, basic_istringstream
#include <stdexcept>                 // for invalid_argument
#include <string>                    // for allocator, operator+, basic_string
#include <string_view>               // for basic_string_view, operator==

namespace fs = std::filesystem;

namespace {
/**
 * @brief Validates whether a string is a valid HTTP method token.
 *
 * @details According to RFC 7230 §3.1.1, HTTP methods are case-sensitive
 *          tokens that may include letters, digits, and a limited set
 *          of special characters:
 *
 *          ```
 *          token = 1*tchar
 *          tchar = "!" / "#" / "$" / "%" / "&" / "'" / "*"
 *                / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
 *                / DIGIT / ALPHA
 *          ```
 *
 *          This function checks that:
 *          - The method is non-empty.
 *          - Every character is alphanumeric or belongs to the allowed set.
 *
 * @param method The candidate method string (e.g. "GET", "POST", "PUT").
 * @return true if the string is a valid HTTP method token, false otherwise.
 */
static bool isValidHttpMethodToken(const std::string& method) {
    if (method.empty())
        return false;

    for (char c : method) {
        unsigned char uc = static_cast<unsigned char>(c);
        // Reject if not alphanumeric and not in the allowed special characters
        if (!std::isalnum(uc) && c != '!' && c != '#' && c != '$' && c != '%' && c != '&' &&
            c != '\'' && c != '*' && c != '+' && c != '-' && c != '.' && c != '^' && c != '_' &&
            c != '`' && c != '|' && c != '~') {
            return false;
        }
    }
    return true;
}

/**
 * @brief Validates that a normalized request path is safe and well-formed.
 *
 * @details This function enforces basic security and syntax rules for
 *          HTTP request targets:
 *          - Must start with a leading `/` (absolute path).
 *          - The root path `/` is always valid.
 *          - Rejects empty paths or those not beginning with `/`.
 *          - Rejects traversal attempts such as `/..`, `/../foo`, or paths ending in `/..`.
 *          - Rejects sequences that attempt to escape the root directory.
 *
 *          Note: Double slashes (`"//"`) were considered for rejection but are
 *          currently allowed (commented out).
 *
 * @param rawPath The request path string (after decoding and normalization).
 * @return true if the path is valid and safe, false otherwise.
 */
static bool isValidPath(const std::string& rawPath) {
    fs::path    p(rawPath);
    std::string s = p.string();

    if (s == "/")
        return true; // root path is always valid
    if (s.empty() || s.front() != '/')
        return false; // must start with "/"

    // Optional stricter check for double slashes (currently disabled)
    /* if (s.find("//") != std::string::npos)
         return false; */

    // Reject directory traversal attempts
    if (s == "/.." || s.find("/../") != std::string::npos || s.ends_with("/.."))
        return false;

    return true;
}

/**
 * @brief Parses a full URL string into its components (HTTP/1.1 version).
 *
 * @details This function is specialized for HTTP/1.1 requests:
 *          - Verifies that the mandatory `Host` header is present when
 *            the request version is `HTTP/1.1` (RFC 7230 §5.4).
 *          - Uses a regular expression to parse the URL into its components:
 *            - Scheme   (e.g., `http`, `https`)
 *            - User     (optional username)
 *            - Password (optional password)
 *            - Host     (domain or IP address)
 *            - Port     (optional numeric port)
 *            - Path     (resource path, e.g., `/index.html`)
 *            - Query    (optional `?param=value` part)
 *            - Fragment (optional `#section` part)
 *          - Throws `std::invalid_argument` if the URL is invalid.
 *
 * @param req The associated @ref HttpRequest, used to validate presence of
 *            the `Host` header when using HTTP/1.1.
 * @param url The raw URL string extracted from the request line.
 *
 * @return A populated @ref Url structure containing parsed fields.
 *
 * @throws std::invalid_argument If the `Host` header is missing (HTTP/1.1)
 *         or the URL does not conform to the expected syntax.
 */
static Url parseUrlHttpVersion1_1(HttpRequest& req, const std::string& url) {
    if (req.getVersion() == "HTTP/1.1" && req.getHeader("HOST").empty()) {
        throw std::invalid_argument("Missing HOST header (required in HTTP/1.1)");
    }
    Url                     res;
    static const std::regex re(
        R"((https?://)?(?:([^:@]+)(?::([^:@]*))?@)?([^:/?#]+)(?::(\d+))?(/[^?#]*)?(?:\?([^#]*))?(?:#(.*))?)");
    std::smatch m;
    if (!std::regex_match(url, m, re)) {
        throw std::invalid_argument("Invalid URL");
    }

    // Assign captured groups to URL components
    res.scheme   = m[1].str();
    res.user     = m[2].str();
    res.password = m[3].str();
    res.host     = m[4].str();
    res.port     = m[5].str();
    res.path     = m[6].str();
    res.query    = m[7].str();
    res.fragment = m[8].str();

    return res;
}

/**
 * @brief Parses a generic URL string into its structured components.
 *
 * @details This function uses a regular expression to extract standard
 *          URL parts from a string. It supports optional components such as:
 *          - Scheme (`http`, `https`)
 *          - User credentials (`user:password`)
 *          - Hostname
 *          - Port
 *          - Path
 *          - Query string
 *          - Fragment identifier
 *
 *          Example:
 *          ```
 *          Url u = parseUrl("https://user:pass@example.com:8080/path/to/file?key=val#frag");
 *          // u.scheme   = "https"
 *          // u.user     = "user"
 *          // u.password = "pass"
 *          // u.host     = "example.com"
 *          // u.port     = "8080"
 *          // u.path     = "/path/to/file"
 *          // u.query    = "key=val"
 *          // u.fragment = "frag"
 *          ```
 *
 * @param url A URL string to parse. It may be absolute or relative,
 *            but must match the expected format.
 *
 * @throws std::invalid_argument If the string does not conform to the
 *         URL grammar.
 *
 * @return A populated @ref Url struct containing the parsed components.
 *
 * @ingroup http
 */
static Url parseUrl(const std::string& url) {
    Url                     res;
    static const std::regex re(
        R"((https?://)?(?:([^:@]+)(?::([^:@]*))?@)?([^:/?#]+)(?::(\d+))?(/[^?#]*)?(?:\?([^#]*))?(?:#(.*))?)");
    std::smatch m;
    if (!std::regex_match(url, m, re)) {
        throw std::invalid_argument("Invalid URL");
    }
    res.scheme   = m[1].str();
    res.user     = m[2].str();
    res.password = m[3].str();
    res.host     = m[4].str();
    res.port     = m[5].str();
    res.path     = m[6].str();
    res.query    = m[7].str();
    res.fragment = m[8].str();

    return res;
}

/**
 * @brief Selects the best-matching server configuration for a given Host header.
 *
 * @details This function determines which @ref Server instance should handle
 *          an incoming request based on the `Host` header:
 *          - If the header includes a port (e.g. `"example.com:8080"`),
 *            the port portion is ignored when matching.
 *          - The function iterates over all provided servers and checks whether
 *            the hostname matches any configured server name (via
 *            @ref Server::hasServerName).
 *          - If a match is found, that server is returned.
 *          - If no match is found, the first server in the list is returned as
 *            the default fallback.
 *
 *          Logging is performed at INFO level to record whether a match
 *          was found or if the fallback server is being used.
 *
 * @param servers    List of @ref Server objects available on the current port.
 * @param hostHeader The raw `Host` header string from the request
 *                   (may include a port suffix, e.g. `"example.com:8080"`).
 *
 * @return A reference to the best-matching @ref Server configuration.
 *
 * @note The caller must ensure that `servers` is non-empty.
 *
 * @ingroup http
 */
static const Server& searchBestMatchedServers(std::vector<Server>& servers,
                                              const std::string&   hostHeader) {
    // Extract hostname (strip port if present)
    std::string hostname = hostHeader;
    size_t      colonPos = hostHeader.find(':');
    if (colonPos != std::string::npos) {
        hostname = hostHeader.substr(0, colonPos); // ignore port in Host format "host:port"
    }

    for (Server& server : servers) {
        if (server.hasServerName(hostname)) {
            Logger::logFrom(LogLevel::INFO, "HttpRequestParser",
                            "Found matching server for Host: " + hostname + " on port " +
                                std::to_string(server.getPort()));
            return server;
        }
    }

    Logger::logFrom(LogLevel::INFO, "HttpRequestParser",
                    "No specific match found, returning default server for Host: " +
                        servers[0].getHost() + " on port " + std::to_string(servers[0].getPort()));
    return servers[0]; // fallback
}
} // namespace

/**
 * @brief Validates and inserts a single HTTP header into the request.
 *
 * @details This function performs centralized header validation and insertion
 *          according to HTTP/1.1 rules and RFC 7230/7231:
 *          - Checks header name validity (non-empty, no control chars).
 *          - Rejects empty values.
 *          - Special validation for `Content-Length`, `Transfer-Encoding`,
 *            and `Expect` headers.
 *          - Enforces conflict rules (e.g., `Content-Length` vs
 *            `Transfer-Encoding`).
 *          - Prevents duplicates for non-mergeable headers (`Host`,
 *            `Content-Type`, etc.).
 *          - Merges values for mergeable headers (per RFC 7230 §3.2.2).
 *
 *          On error, an appropriate HTTP status code is set in
 *          `errorCode` (e.g., 400 Bad Request, 411 Length Required,
 *          417 Expectation Failed).
 *
 * @param req       Reference to the @ref HttpRequest being populated.
 * @param key       Header field name (case-insensitive).
 * @param value     Header field value.
 * @param errorCode Output parameter: set to HTTP error code if validation fails.
 *
 * @return `true` if the header was successfully validated and inserted,
 *         `false` otherwise (with `errorCode` set).
 *
 * @ingroup http
 */
bool insertValidatedHeader(HttpRequest& req, const std::string& key, const std::string& value,
                           int& errorCode) {
    std::string normKey = toUpper(key);

    // 1) Validate header name: must not be empty or contain control chars
    if (normKey.empty() || std::any_of(normKey.begin(), normKey.end(), [](char c) {
            unsigned char uc = static_cast<unsigned char>(c);
            return uc <= 0x1F || uc == 0x7F;
        })) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Invalid or empty header name: " + normKey);
        errorCode = 400;
        return false;
    }

    // 2) Reject empty header values
    if (value.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Header missing value: " + normKey);
        errorCode = 400;
        return false;
    }

    bool first = !req.hasHeader(normKey);

    // 3) Special case: Content-Length must be numeric and unique
    if (normKey == "CONTENT-LENGTH") {
        if (!std::all_of(value.begin(), value.end(),
                         [](char c) { return std::isdigit(static_cast<unsigned char>(c)); })) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Invalid Content-Length value: " + value);
            errorCode = 411;
            return false;
        }
        unsigned long long len;
        try {
            len = std::stoull(value);
        } catch (...) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Invalid Content-Length value: " + value);
            errorCode = 411;
            return false;
        }
        req.setContentLength(len);
    }

    // 4) Special case: Expect header → reject 100-continue
    if (normKey == "EXPECT") {
        std::string lower = toLower(value);
        if (lower == "100-continue") {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Unsupported Expect header: " + value);
            errorCode = 417;
            return false;
        }
    }

    // 5) Special case: Transfer-Encoding → only "chunked" allowed
    if (normKey == "TRANSFER-ENCODING") {
        std::string lower = toLower(value);
        if (lower != "chunked") {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Unsupported Transfer-Encoding: " + lower);
            errorCode = 501;
            return false;
        }
        if (req.getMethod() == "GET") {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Chunked Transfer-Encoding not allowed for GET");
            errorCode = 400;
            return false;
        }
    }

    // 6) Check for Content-Length ↔ Transfer-Encoding conflict
    if (first) {
        if ((normKey == "CONTENT-LENGTH" && !req.getHeader("TRANSFER-ENCODING").empty()) ||
            (normKey == "TRANSFER-ENCODING" && req.getContentLength() > 0)) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Both Content-Length and Transfer-Encoding present");
            errorCode = 400;
            return false;
        }
    }

    // 7) Handle duplicates: some headers must not be repeated
    static const std::set<std::string> nonMergeable = {
        "HOST", "CONTENT-LENGTH", "CONTENT-TYPE", "TRANSFER-ENCODING", "EXPECT", "CONNECTION"};

    if (!first) {
        if (nonMergeable.count(normKey)) {
            // Special case: identical Content-Length values are tolerated
            if (normKey == "CONTENT-LENGTH") {
                if (req.getHeader(normKey) != value) {
                    Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                                    "Conflicting Content-Length headers");
                    errorCode = 400;
                    return false;
                }
                return true;
            }
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Duplicate " + normKey + " header");
            errorCode = 400;
            return false;
        }

        // Otherwise, merge values (e.g., Accept: a, b)
        req.setHeader(normKey, req.getHeader(normKey) + ", " + value);
    } else {
        req.setHeader(normKey, value);
    }

    return true;
}

/**
 * @brief Parses and validates the request line and headers of an HTTP request.
 *
 * @details This function processes the "head" section of an HTTP request,
 *          which includes the request line (method, target, version) and all
 *          header fields. It performs:
 *
 *          - Validation of the request line:
 *            • Ensures it contains exactly 3 fields (method, target, version).
 *            • Validates that the method token is syntactically valid.
 *            • Rejects empty fields.
 *          - Decoding and validation of the request-target:
 *            • Percent-decoding.
 *            • Rejects control characters.
 *            • Forbids absolute-URIs and fragments (`#...`).
 *            • Splits query parameters from the path.
 *            • Enforces maximum URI length.
 *            • Normalizes the path and blocks traversal attempts (`..`).
 *          - Header parsing:
 *            • Reads each header line, ensures proper format (`key: value`).
 *            • Delegates validation/merging logic to @ref insertValidatedHeader.
 *          - URL building:
 *            • Uses the `Host` header and matched @ref Server configuration.
 *            • Sets normalized host and stores full @ref Url in the request.
 *
 *          On error, this function sets `errorCode` to the appropriate HTTP
 *          status code (e.g., 400, 403, 414, 505) and returns `false`.
 *
 * @param req              Reference to the @ref HttpRequest being populated.
 * @param headerPart       Raw string containing the request line + headers.
 * @param errorCode        Output parameter: set if parsing fails.
 * @param servers          List of servers listening on the same port (for
 *                         Host/ServerName matching).
 * @param clientMaxBodySize Output parameter: max body size derived from the
 *                         selected server configuration.
 *
 * @return `true` if parsing succeeds and `req` is populated,
 *         `false` otherwise (with `errorCode` set).
 *
 * @ingroup http
 */
bool parseReqHeader(HttpRequest& req, const std::string& headerPart, int& errorCode,
                    std::vector<Server> servers, size_t& clientMaxBodySize) {
    std::istringstream stream(headerPart);
    std::string        line;

    // 1) Parse request line (must have exactly 3 parts)
    if (!std::getline(stream, line) || line.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Empty or missing request start line");
        errorCode = 400;
        return false;
    }
    size_t sp1 = line.find(' ');
    size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);
    size_t sp3 = sp2 == std::string::npos ? std::string::npos : line.find(' ', sp2 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos || sp3 != std::string::npos) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Malformed request line (must have exactly two spaces)");
        errorCode = 400;
        return false;
    }

    std::string method    = line.substr(0, sp1);
    std::string rawTarget = line.substr(sp1 + 1, sp2 - sp1 - 1);
    std::string version   = trim(line.substr(sp2 + 1));

    // Validate method and basic request line fields
    if (!isValidHttpMethodToken(method)) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Method Not Allowed: " + method);
        errorCode = 400;
        return false;
    }
    if (method.empty() || rawTarget.empty() || version.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid request start line fields");
        errorCode = 400;
        return false;
    }

    // 2) Decode and validate path
    std::string decoded;
    try {
        decoded = decodePercentEncoding(rawTarget);
    } catch (const std::exception& e) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", e.what());
        errorCode = 400;
        return false;
    }
    for (unsigned char c : decoded) {
        if (std::iscntrl(c)) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Path contains control characters");
            errorCode = 400;
            return false;
        }
    }

    // Forbid absolute URIs (http://...) and fragments (#...)
    if (decoded.rfind("http://", 0) == 0 || decoded.rfind("https://", 0) == 0) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Rejected absolute-URI request-target: " + decoded);
        errorCode = 400;
        return false;
    }
    size_t hashPos = decoded.find('#');
    if (hashPos != std::string::npos) {
        decoded.erase(hashPos); // strip fragment
    }

    // Split into path and query string
    std::string pathOnly = decoded, query;
    size_t      qpos     = decoded.find('?');
    if (qpos != std::string::npos) {
        pathOnly = decoded.substr(0, qpos);
        query    = decoded.substr(qpos + 1);
    }

    // Reject overly long URIs
    static constexpr size_t MAX_URI_LEN = 2048;
    if (pathOnly.size() > MAX_URI_LEN) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Request-URI Too Long: " + pathOnly);
        errorCode = 414;
        return false;
    }

    // 3) Initialize request object
    req = HttpRequest();
    req.setMethod(method);

    // Reject traversal attempts via ".." segments
    {
        std::istringstream segstream(pathOnly);
        std::string        seg;
        while (std::getline(segstream, seg, '/')) {
            if (seg == "..") {
                Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                                "Rejected traversal attempt: " + pathOnly);
                errorCode = 403;
                return false;
            }
        }
    }

    // Normalize path and set fields
    std::string norm = normalizePath(pathOnly);
    if (norm.empty()) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Path escapes root: " + pathOnly);
        errorCode = 403;
        return false;
    }
    req.setPath(norm);
    req.setQuery(query);
    req.setVersion(version);

    // Only support HTTP/1.0 and HTTP/1.1
    if (version != "HTTP/1.0" && version != "HTTP/1.1") {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid HTTP version: " + version);
        errorCode = 505;
        return false;
    }

    // 4) Parse headers line by line
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0 || colon + 1 >= line.size()) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Malformed header line: " + line);
            errorCode = 400;
            return false;
        }

        std::string key = line.substr(0, colon);
        if (key.empty() || key.find_first_of(" \t") != std::string::npos) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid header name: " + key);
            errorCode = 400;
            return false;
        }

        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        // Delegate validation/insertion to helper
        if (!insertValidatedHeader(req, key, value, errorCode))
            return false;
    }

    // 5) Build full URL using Host header + server config
    std::string hostHeader = req.getHeader("HOST");
    try {
        Url           url;
        const Server& foundServer = searchBestMatchedServers(servers, hostHeader);
        clientMaxBodySize         = foundServer.getClientMaxBodySize();

        std::string urlStr =
            "http://" + (hostHeader.empty() ? foundServer.getHost() : hostHeader) + req.getPath();

        // Use stricter parsing for HTTP/1.1
        if (req.getVersion() == "HTTP/1.1") {
            url = parseUrlHttpVersion1_1(req, urlStr);
        } else {
            url = parseUrl(urlStr);
        }

        req.setUrl(url);

        // Normalize and set host explicitly
        std::string& headerAfterParseUrl = url.host;
        std::transform(headerAfterParseUrl.begin(), headerAfterParseUrl.end(),
                       headerAfterParseUrl.begin(), ::tolower);
        req.setHost(headerAfterParseUrl);

        Logger::logFrom(LogLevel::INFO, "HttpRequestParser",
                        "Parsed URL: " + urlStr + " with host: " + headerAfterParseUrl);
    } catch (const std::exception& e) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", e.what());
        errorCode = 400;
        return false;
    }

    return true;
}

/**
 * @brief Checks if a chunked HTTP request body is complete.
 *
 * @details According to RFC 7230 §4.1, a chunked transfer ends when:
 *          - A zero-length chunk (`0\r\n`) is received.
 *          - Followed by an optional trailer section.
 *          - And terminated by a blank line (`\r\n\r\n`).
 *
 *          This function inspects the body buffer to determine if the final
 *          zero-size chunk and terminating CRLF sequence are present.
 *
 * @param bodyPart The raw body data received so far.
 * @return `true` if the chunked body is complete,
 *         `false` if more data is required.
 *
 * @ingroup http
 */
bool isChunkedBodyComplete(const std::string& bodyPart) {
    // 1. Look for the "0\r\n" marker (start of the last chunk).
    std::size_t zeroPos = bodyPart.find("0\r\n");
    if (zeroPos == std::string::npos)
        return false; // no terminator chunk yet

    // 2. Ensure the trailers (if any) are properly terminated with CRLFCRLF.
    std::size_t trailerEnd = bodyPart.find("\r\n\r\n", zeroPos);
    return trailerEnd != std::string::npos;
}

/**
 * @brief Parses a chunked HTTP request body and assembles it into the request.
 *
 * @details Implements RFC 7230 §4.1 "Chunked Transfer Coding".
 *          - Reads each chunk size (hexadecimal) from the body stream.
 *          - Copies the corresponding number of bytes into a contiguous body buffer.
 *          - Stops when a zero-size chunk is encountered.
 *          - Rejects trailers (unsupported in this implementation).
 *          - Enforces the per-client maximum body size limit.
 *
 * @param req              Reference to the @ref HttpRequest being built.
 * @param bodyPart         The raw body data received so far (may include CRLFs, chunks, trailers).
 * @param clientMaxBodySize Maximum allowed body size for this client (bytes).
 * @param errorCode        Output parameter: set to an HTTP error status (e.g. 400, 413) on failure.
 * @param consumedBytes    Output parameter: number of bytes successfully consumed from the buffer.
 *
 * @note If trailers are present after the final 0–chunk, the request is rejected with `400`.
 * @note If the accumulated body exceeds @p clientMaxBodySize, the request is rejected with `413`.
 *
 * @ingroup http
 */
void chunkReqHandler(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                     int& errorCode, std::size_t& consumedBytes) {
    std::istringstream stream(bodyPart);
    std::string        line;
    std::string        fullBody;
    std::size_t        total = 0, local = 0;

    // Process each line (either chunk size, CRLF, or trailers)
    while (std::getline(stream, line)) {
        local += line.size() + 1; // count CRLF too
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // 1) Parse chunk size (hexadecimal)
        std::size_t chunkSize;
        try {
            chunkSize = std::stoul(line, nullptr, 16);
        } catch (...) {
            errorCode = 400; // Bad Request: malformed chunk size
            consumedBytes += local;
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Invalid chunk size format");
            return;
        }

        // 2) Final 0–chunk indicates end of body
        if (chunkSize == 0) {
            // Consume the CRLF after the last chunk
            std::getline(stream, line);
            local += line.size() + 1;

            // Reject any trailers (not supported)
            std::string trailer;
            while (std::getline(stream, trailer)) {
                local += trailer.size() + 1;
                if (trailer.empty())
                    break;       // empty line terminates trailers
                errorCode = 400; // reject any non-empty trailer
                consumedBytes += local;
                Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                                "Unsupported trailer header: " + trailer);
                return;
            }
            break;
        }

        // 3) Enforce max body size limit
        Logger::logFrom(LogLevel::WARN, "PARSER",
                        " Totalsize: " + std::to_string(total + chunkSize));
        if (total + chunkSize > clientMaxBodySize) {
            errorCode = 413; // Payload Too Large
            consumedBytes += local + chunkSize;
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Exceeded max body size in chunked transfer");
            return;
        }

        // 4) Read the chunk data
        std::string data(chunkSize, '\0');
        stream.read(&data[0], chunkSize);
        std::streamsize got = stream.gcount();
        local += got;
        fullBody.append(data, 0, got);
        total += got;

        // Consume CRLF after the chunk data
        std::getline(stream, line);
        local += line.size() + 1;
    }

    // 5) Finalize request
    req.setBody(fullBody);
    req.setContentLength(fullBody.size());
    errorCode = 0;
    consumedBytes += local;
}

/**
 * @brief Heuristically checks if a string looks like the start line of an HTTP request.
 *
 * @details This helper is used to detect pipelined requests:
 *          - It tests whether the string begins with one of the known HTTP
 *            method tokens followed by a space (e.g., "GET ", "POST ").
 *          - Only a small subset of standard methods is recognized
 *            (GET, POST, DELETE, PUT, HEAD, OPTIONS, PATCH).
 *
 *          This is not a full request-line parser, but a lightweight
 *          heuristic to distinguish request boundaries in a raw buffer.
 *
 * @param s The string segment to test (usually leftover buffer data).
 * @return true if @p s looks like a valid request start line, false otherwise.
 *
 * @ingroup http
 */
static bool isLikelyStartLine(const std::string& s) {
    // List of method tokens followed by a space
    static const std::array<std::string_view, 7> methods = {"GET ",  "POST ",    "DELETE ", "PUT ",
                                                            "HEAD ", "OPTIONS ", "PATCH "};

    // Compare the beginning of s with each known method token
    for (auto m : methods) {
        if (s.size() >= m.size() && std::string_view(s).substr(0, m.size()) == m) {
            return true; // Found a match → likely start line
        }
    }

    return false; // No match → probably not a start line
}

/**
 * @brief Parses and validates the HTTP request body according to Content-Length or
 * Transfer-Encoding.
 *
 * @details This function handles two body transfer mechanisms:
 *          - **Chunked transfer encoding**: waits until the terminating `0\r\n\r\n` is received,
 *            then delegates parsing to @ref chunkReqHandler. Rejects incomplete or malformed
 *            chunked bodies.
 *          - **Fixed-length bodies**: uses the `Content-Length` header to determine how many bytes
 *            to consume, rejecting requests that exceed @p clientMaxBodySize or contain mismatched
 *            lengths.
 *
 *          It also supports HTTP/1.1 pipelining:
 *          - If extra bytes follow a complete body, the function checks whether they look like the
 *            start of another request (via @ref isLikelyStartLine).
 *          - If so, only the declared body is consumed and parsing can continue with the next
 * request.
 *
 * @param req               The @ref HttpRequest object being filled.
 * @param bodyPart          Raw body data extracted from the client buffer.
 * @param clientMaxBodySize Maximum allowed body size (from server configuration).
 * @param errorCode         Reference set to HTTP error code on failure (0 on success).
 * @param consumedBytes     Reference updated with the number of bytes consumed from @p bodyPart.
 *
 * @return true if the body is complete and valid, false if more data is needed or if an error
 * occurred.
 *
 * @ingroup http
 */
bool parseReqBody(HttpRequest& req, const std::string& bodyPart, std::size_t clientMaxBodySize,
                  int& errorCode, std::size_t& consumedBytes) {
    const std::string& te = req.getHeader("TRANSFER-ENCODING");

    // 1) Handle chunked transfer encoding
    if (te == "chunked") {
        if (!isChunkedBodyComplete(bodyPart)) {
            // Body not yet complete → wait for more data
            Logger::logFrom(LogLevel::INFO, "HttpRequestParser",
                            "Incomplete chunked body, waiting for more");
            errorCode = 0;
            return false;
        }
        // Fully received → delegate to chunked handler
        chunkReqHandler(req, bodyPart, clientMaxBodySize, errorCode, consumedBytes);
        return (errorCode == 0);
    }

    // 2) Handle fixed-length bodies
    std::size_t len = req.getContentLength();

    // Reject if declared length exceeds configured max
    if (len > 0 && len >= clientMaxBodySize) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Exceeded max body size in non-chunked transfer");
        errorCode = 413; // Payload Too Large
        consumedBytes += bodyPart.size();
        return false;
    }

    // Not enough data yet → wait for more
    if (bodyPart.size() < len) {
        errorCode = 0;
        return false;
    }

    // 3) Detect pipelined requests
    if (bodyPart.size() > len) {
        // Look at the bytes immediately after the declared body
        std::string_view remainder(bodyPart.c_str() + len, bodyPart.size() - len);

        if (isLikelyStartLine(std::string(remainder))) {
            // Treat as pipelined request → consume only declared body
            req.setBody(bodyPart.substr(0, len));
            consumedBytes += len;
            errorCode = 0;
            return true;
        }

        // Extra data is invalid → reject
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Content-Length mismatch: body too long");
        errorCode = 400; // Bad Request
        consumedBytes += bodyPart.size();
        return false;
    }

    // 4) Exact match → accept body
    req.setBody(bodyPart.substr(0, len));
    consumedBytes += len;
    errorCode = 0;
    return true;
}

/**
 * @brief Performs final validation of a parsed HTTP request.
 *
 * @details This function applies protocol-level validation rules after the request line
 *          and headers have been parsed. Specifically:
 *          - Ensures the HTTP method is supported (GET, POST, DELETE).
 *          - Validates the normalized request path (must not escape root).
 *          - Checks the `Connection` header (only "keep-alive" or "close" are accepted).
 *          - For POST requests:
 *              * Requires either a `Content-Length` or `Transfer-Encoding` header.
 *              * Requires a valid `Content-Type` header, chosen from a limited set
 *                (supports parameters like `multipart/form-data; boundary=...`).
 *
 * @param req       The @ref HttpRequest object to validate.
 * @param errorCode Reference set to HTTP error code if validation fails.
 *
 * @return true if the request is valid, false otherwise (with @p errorCode set).
 *
 * @ingroup http
 */
bool validateReq(HttpRequest& req, int& errorCode) {
    // Allowed methods
    static const std::set<std::string> methods = {"GET", "POST", "DELETE"};

    if (!methods.count(req.getMethod())) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Method Not Allowed: " + req.getMethod());
        errorCode = 405; // NOTE: semantically could be 501, but 405 is used for tests
        return false;
    }

    // Path validation
    if (!isValidPath(req.getPath())) {
        Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                        "Invalid request path: " + req.getPath());
        errorCode = 403; // Forbidden
        return false;
    }

    // Connection header
    std::string conn = req.getHeader("CONNECTION");
    if (!conn.empty()) {
        std::string lower = toLower(conn);
        if (lower != "keep-alive" && lower != "close") {
            // Normalize invalid values to "close" but still reject
            req.setHeader("CONNECTION", "close");
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Invalid Connection header value: " + conn);
            errorCode = 400; // Bad Request
            return false;
        }
    }

    // POST-specific checks
    if (req.getMethod() == "POST") {
        // Must declare either Content-Length or Transfer-Encoding
        if (req.getContentLength() == 0 && req.getHeader("TRANSFER-ENCODING").empty()) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "POST without Content-Length or Transfer-Encoding");
            errorCode = 411; // Length Required
            return false;
        }

        // Require Content-Type
        std::string ct = req.getHeader("CONTENT-TYPE");
        if (ct.empty()) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser", "Missing Content-Type for POST");
            errorCode = 415; // Unsupported Media Type
            return false;
        }

        // Allowed Content-Types (prefix matching for params like boundary=...)
        static const std::set<std::string> validTypes = {"application/x-www-form-urlencoded",
                                                         "multipart/form-data",
                                                         "text/plain",
                                                         "application/json",
                                                         "application/octet-stream",
                                                         "test/file"};

        std::string lowerCt = toLower(ct);
        bool        valid   = false;
        for (const std::string& type : validTypes) {
            if (lowerCt.rfind(type, 0) == 0) { // prefix match
                valid = true;
                break;
            }
        }

        if (!valid) {
            Logger::logFrom(LogLevel::ERROR, "HttpRequestParser",
                            "Unsupported Content-Type for POST: " + ct);
            errorCode = 415; // Unsupported Media Type
            return false;
        }
    }

    return true;
}

/**
 * @brief Parses a raw HTTP request buffer into an @ref HttpRequest object.
 *
 * @details This is the main entry point for HTTP request parsing. It coordinates
 *          header parsing, body parsing, and final validation. The workflow is:
 *
 *          1. Locate the end of the header section (`\r\n\r\n`).
 *          2. Split the buffer into a header block and a body block.
 *          3. Parse the request line and headers via @ref parseReqHeader.
 *          4. Validate the parsed request with @ref validateReq.
 *          5. If the method does not expect a body (e.g. GET/DELETE), exit early.
 *             - Detect and support HTTP pipelining (multiple requests in one buffer).
 *          6. Otherwise, parse the body with @ref parseReqBody.
 *          7. On success, return a fully populated @ref HttpRequest.
 *
 *          Error handling:
 *          - If parsing fails, the function sets @p errorCode to the appropriate
 *            HTTP status code (400, 405, 411, 413, 414, 415, 505, etc.).
 *          - @p consumedBytes is always updated to reflect how much of the buffer
 *            was processed (even if parsing fails).
 *
 * @param req            The @ref HttpRequest to populate.
 * @param buffer         Raw input buffer containing HTTP request(s).
 * @param serversOnPort  List of available @ref Server objects for virtual-host matching.
 * @param errorCode      Output parameter set to HTTP error code on failure.
 * @param consumedBytes  Output parameter set to the number of bytes successfully consumed.
 *
 * @return true if the request was successfully parsed, false otherwise.
 *
 * @ingroup http
 */
bool HttpRequestParser::parse(HttpRequest& req, const std::string& buffer,
                              std::vector<Server> serversOnPort, int& errorCode,
                              std::size_t& consumedBytes) {
    // 1) Find end of header block
    std::size_t headerEndPos = buffer.find("\r\n\r\n");
    if (headerEndPos == std::string::npos) {
        Logger::logFrom(LogLevel::INFO, "HttpRequestParser", "Incomplete header, waiting for more");
        errorCode = 0; // Not an error, just incomplete
        return false;
    }
    std::size_t headerLen = headerEndPos + 4;

    // 2) Split buffer into header and body
    std::string headerPart = buffer.substr(0, headerLen);
    std::string bodyPart   = buffer.substr(headerLen);
    consumedBytes          = headerLen;

    // 3) Parse request line + headers
    std::size_t clientMaxBodySize;
    if (!parseReqHeader(req, headerPart, errorCode, serversOnPort, clientMaxBodySize)) {
        return false; // parseReqHeader sets errorCode
    }

    // 4) Validate request semantics
    if (!validateReq(req, errorCode)) {
        return false; // validateReq sets errorCode
    }

    // 5) Early exit for bodyless methods (GET, DELETE)
    std::string method = req.getMethod();
    if (method == "GET") {
        if (buffer.size() == headerLen) {
            // Clean GET: no extra data
            consumedBytes = headerLen;
            errorCode     = 0;
            return true;
        }

        // Check if extra data looks like a new request (pipelining)
        char next = buffer[headerLen];
        if (next >= 'A' && next <= 'Z') {
            // Treat as pipelined request
            consumedBytes = headerLen;
            errorCode     = 0;
            return true;
        }

        // Otherwise, ignore any stray "body"
        consumedBytes = buffer.size();
        errorCode     = 0;
        return true;
    }

    // 6) Parse body for methods that require it (e.g. POST)
    std::size_t bodyConsumed = 0;
    bool        bodyOk = parseReqBody(req, bodyPart, clientMaxBodySize, errorCode, bodyConsumed);
    if (!bodyOk) {
        consumedBytes = headerLen + bodyConsumed;
        return false; // parseReqBody sets errorCode
    }

    // 7) Success
    consumedBytes = headerLen + bodyConsumed;
    errorCode     = 0;
    return true;
}
