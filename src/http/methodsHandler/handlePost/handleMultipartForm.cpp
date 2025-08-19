/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleMultipartForm.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 21:44:51 by nlouis            #+#    #+#             */
/*   Updated: 2025/08/19 11:24:48 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * @file    handleMultipartForm.cpp
 * @brief   Handles `multipart/form-data` POST uploads and persists the first file part to disk.
 *
 * @details
 * Parses the request body using the `boundary` from the `Content-Type` header,
 * scans parts until it finds a file part (`filename="..."`), trims the trailing
 * CRLF from the part payload, enforces `client_max_body_size`, and writes the
 * bytes to `<upload_dir>/<sanitized-filename>` (or a time‑based fallback).
 *
 * ### Flow
 * 1. Extract `boundary` from `Content-Type`; `400` if missing.
 * 2. `parseMultipartForm`: iterate parts, split headers/data, detect `filename=`,
 *    sanitize file name, trim payload CRLF, return name + bytes.
 * 3. Enforce per‑part size ≤ `Server::getClientMaxBodySize()`; `413` otherwise.
 * 4. Open destination file (binary) and write bytes atomically; `500` on I/O error.
 * 5. Log outcome and return `201 Created` with a minimal HTML confirmation.
 *
 * ### Security & Robustness
 * - **Filename sanitization**: strips unsafe characters to prevent path traversal.
 * - **Size limits**: rejects oversized payloads to mitigate DoS.
 * - **Strict delimiter matching**: avoids bleed between parts.
 *
 * ### Limitations
 * - Parses the **first** file part only (ignores subsequent files/fields).
 * - Operates in‑memory (no streaming/chunking); large files may increase RAM use.
 *
 * @note Consider extending to stream to a temp file, support multiple files/fields,
 *       and return JSON for API clients.
 *
 * @see HttpRequest, HttpResponse, ResponseBuilder, Logger, filesystemUtils
 * @ingroup request_handler
 */

#include "core/Server.hpp"           // for Server
#include "http/HttpRequest.hpp"      // for HttpRequest
#include "http/HttpResponse.hpp"     // for HttpResponse
#include "http/responseBuilder.hpp"  // for generateError, generateSuccess
#include "utils/Logger.hpp"          // for LogLevel, Logger
#include "utils/filesystemUtils.hpp" // for getCurrentTime, joinPath, sanit...
#include "utils/htmlUtils.hpp"       // for htmlEscape
#include <algorithm>                 // for min
#include <fstream>                   // for basic_ofstream, basic_ostream
#include <stddef.h>                  // for size_t
#include <string>                    // for allocator, string, operator+
#include <utility>                   // for move

namespace {

/**
 * @brief Construct the opening delimiter for a multipart part.
 *
 * @param boundary Boundary token from the Content-Type header.
 * @return A delimiter string in the form `"--<boundary>\r\n"`.
 */
inline std::string makePartDelimiter(const std::string& boundary) {
    return "--" + boundary + "\r\n";
}

/**
 * @brief Construct the closing delimiter for a multipart body.
 *
 * @param boundary Boundary token from the Content-Type header.
 * @return A delimiter string in the form `"--<boundary>--"`.
 */
inline std::string makeCloseDelimiter(const std::string& boundary) {
    return "--" + boundary + "--";
}

/**
 * @brief Find the next occurrence of a delimiter in a multipart body.
 *
 * @param body      Full request body.
 * @param delimiter Delimiter string to search for.
 * @param startPos  Starting offset in the body.
 * @param outPos    Output parameter; set to the position of the delimiter if found.
 * @return true if the delimiter was found, false otherwise.
 */
bool findNextDelimiter(const std::string& body, const std::string& delimiter, size_t startPos,
                       size_t& outPos) {
    outPos = body.find(delimiter, startPos);
    return outPos != std::string::npos;
}

/**
 * @brief Check if the body contains the closing delimiter at the given position.
 *
 * @param body           Full request body.
 * @param pos            Position to check.
 * @param closeDelimiter Expected closing delimiter string.
 * @return true if the body matches the closing delimiter at `pos`.
 */
bool isCloseDelimiter(const std::string& body, size_t pos, const std::string& closeDelimiter) {
    return body.compare(pos, closeDelimiter.size(), closeDelimiter) == 0;
}

/**
 * @brief Split a raw part into headers and data sections.
 *
 * @param part        Raw part (between delimiters).
 * @param headersBlock Output; substring up to the first double CRLF.
 * @param dataBlock    Output; substring after the first double CRLF.
 * @return true if headers and data were successfully split, false otherwise.
 */
bool parseHeadersAndData(const std::string& part, std::string& headersBlock,
                         std::string& dataBlock) {
    size_t headerEnd = part.find("\r\n\r\n");
    if (headerEnd == std::string::npos)
        return false;

    headersBlock = part.substr(0, headerEnd);
    dataBlock    = part.substr(headerEnd + 4); // skip past "\r\n\r\n"
    return true;
}

/**
 * @brief Extract and sanitize the filename from a part's headers.
 *
 * @param headersBlock Headers of the multipart part.
 * @param outFilename  Output; sanitized filename if found.
 * @return true if a `filename="..."` token was found, false otherwise.
 */
bool extractFilename(const std::string& headersBlock, std::string& outFilename) {
    const std::string token = "filename=\"";
    auto              pos   = headersBlock.find(token);
    if (pos == std::string::npos)
        return false;

    pos += token.size();
    auto endQuote = headersBlock.find('"', pos);
    if (endQuote == std::string::npos)
        return false;

    // 1. Extract the raw filename
    std::string rawName = headersBlock.substr(pos, endQuote - pos);

    // 2. Sanitize it before returning
    outFilename = sanitizeFilename(rawName);
    return true;
}

/**
 * @brief Strip a trailing CRLF sequence from data if present.
 *
 * @param data Raw part data block.
 * @return A copy of `data` without a trailing `"\r\n"`, if it existed.
 */
std::string trimEndingCRLF(const std::string& data) {
    if (data.size() >= 2 && data[data.size() - 2] == '\r' && data[data.size() - 1] == '\n') {
        return data.substr(0, data.size() - 2);
    }
    return data;
}

/**
 * @brief Parse a multipart/form-data body and extract the first file part.
 *
 * @details
 * Iterates over all parts in the request body, delimited by the given boundary.
 * For each part:
 * - Splits headers and data at the first double CRLF.
 * - Checks for a `filename="..."` header.
 * - If found, sanitizes the filename, trims trailing CRLF from the data block,
 *   and outputs both.
 *
 * Stops at the first file part encountered; ignores non-file parts.
 *
 * @param body        Full HTTP request body (including all parts).
 * @param boundary    Boundary token from the Content-Type header.
 * @param filename    Output; sanitized filename from the file part.
 * @param fileContent Output; raw file content (as bytes in a std::string).
 *
 * @return true if a file part was found and parsed successfully, false otherwise.
 *
 * @note Only the first file part is extracted; additional files are ignored.
 * @warning Operates in memory; large uploads may impact RAM usage.
 */
bool parseMultipartForm(const std::string& body, const std::string& boundary, std::string& filename,
                        std::string& fileContent) {

    const std::string partDelimiter  = makePartDelimiter(boundary);
    const std::string closeDelimiter = makeCloseDelimiter(boundary);

    size_t curPos = 0;
    while (true) {
        // 1) Locate the start of the next part:
        size_t partStart;
        if (!findNextDelimiter(body, partDelimiter, curPos, partStart)) {
            // No more "--boundary\r\n", so nothing to parse
            return false;
        }

        // 2) Check if this is actually the closing delimiter:
        if (isCloseDelimiter(body, partStart, closeDelimiter)) {
            return false;
        }

        // Move past the delimiter header line:
        size_t contentStart = partStart + partDelimiter.size();

        // 3) Find end of this part (either next partDelimiter or closeDelimiter):
        size_t nextPartPos = std::string::npos;
        findNextDelimiter(body, partDelimiter, contentStart, nextPartPos);

        size_t closePos = std::string::npos;
        findNextDelimiter(body, closeDelimiter, contentStart, closePos);

        size_t partEnd;
        if (nextPartPos == std::string::npos && closePos == std::string::npos) {
            // Malformed: no closing boundary for this part
            return false;
        } else if (nextPartPos == std::string::npos) {
            partEnd = closePos;
        } else if (closePos == std::string::npos) {
            partEnd = nextPartPos;
        } else {
            partEnd = std::min(nextPartPos, closePos);
        }

        // 4) Extract the raw bytes of this part (excluding the delimiter itself):
        std::string part = body.substr(contentStart, partEnd - contentStart);

        // 5) Split into headers vs. data:
        std::string headersBlock, dataBlock;
        if (!parseHeadersAndData(part, headersBlock, dataBlock)) {
            return false;
        }

        // 6) Look for a filename="…". If not found, skip to next part:
        std::string extractedName;
        if (!extractFilename(headersBlock, extractedName)) {
            curPos = partEnd;
            continue; // keep scanning subsequent parts
        }

        // 7) We found a file‐upload part. Trim trailing CRLF from data:
        filename    = std::move(extractedName);
        fileContent = trimEndingCRLF(dataBlock);
        return true;
    }

    // (unreachable)
    return false;
}

} // anonymous namespace

/**
 * @brief Handle a multipart/form-data POST request and persist the uploaded file.
 *
 * @details
 * - Extracts the boundary from the `Content-Type` header.
 * - Parses the multipart body to find the first file part.
 * - Enforces `client_max_body_size` to prevent DoS.
 * - Generates a fallback filename if none was provided.
 * - Writes the file content to disk under the given upload directory.
 * - Returns an appropriate `HttpResponse`:
 *   - 400 if malformed,
 *   - 413 if too large,
 *   - 500 if I/O fails,
 *   - 201 with a confirmation HTML page on success.
 *
 * @param request     Incoming HTTP request containing the multipart body.
 * @param server      Server configuration (used for limits and error pages).
 * @param fullDirPath Absolute path of the directory where uploads are stored.
 *
 * @return HttpResponse representing the outcome (success or error).
 *
 * @ingroup request_handler
 */
HttpResponse handleMultipartForm(const HttpRequest& request, const Server& server,
                                 const std::string& fullDirPath) {
    // 1) Extract boundary from Content-Type header
    std::string contentType = request.getHeader("Content-Type");
    size_t      bpos        = contentType.find("boundary=");
    if (bpos == std::string::npos) {
        Logger::logFrom(LogLevel::WARN, "handleMultipartForm", "Missing boundary in Content-Type");
        return ResponseBuilder::generateError(400, server, request);
    }
    std::string boundary = contentType.substr(bpos + 9);

    // 2) Parse body to extract filename + file content
    std::string extractedFilename;
    std::string fileContent;
    if (!parseMultipartForm(request.getBody(), boundary, extractedFilename, fileContent)) {
        Logger::logFrom(LogLevel::WARN, "handleMultipartForm", "Failed to parse multipart form");
        return ResponseBuilder::generateError(400, server, request);
    }

    // 3) Enforce per-part size limit
    size_t maxBody = server.getClientMaxBodySize();
    if (fileContent.size() > maxBody) {
        Logger::logFrom(LogLevel::WARN, "handleMultipartForm",
                        "Uploaded part size " + std::to_string(fileContent.size()) +
                            " exceeds max client body size " + std::to_string(maxBody));
        return ResponseBuilder::generateError(413, server, request);
    }

    // 4) Fallback filename if client didn’t provide one
    if (extractedFilename.empty()) {
        extractedFilename = "upload_" + std::to_string(getCurrentTime());
    }

    // 5) Compute destination path and open file
    std::string   fullpath = joinPath(fullDirPath, extractedFilename);
    std::ofstream out(fullpath, std::ios::binary);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "handleMultipartForm",
                        "Failed to open file for writing: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    // 6) Write content and verify
    out << fileContent;
    out.close();
    if (out.fail()) {
        Logger::logFrom(LogLevel::ERROR, "handleMultipartForm",
                        "Failed to write file content to: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    // 7) Success → return confirmation page
    Logger::logFrom(LogLevel::INFO, "handleMultipartForm", "Successfully uploaded: " + fullpath);
    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>Uploaded: " + htmlEscape(extractedFilename) + "</h1></body></html>",
        "text/html", request);
}
