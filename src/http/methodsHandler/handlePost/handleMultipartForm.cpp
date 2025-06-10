/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   handleMultipartForm.cpp                            :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: nlouis <nlouis@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/06/06 21:44:51 by nlouis            #+#    #+#             */
/*   Updated: 2025/06/10 21:48:10 by nlouis           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "core/Server.hpp"
#include "http/HttpRequest.hpp"
#include "http/responseBuilder.hpp"
#include "utils/Logger.hpp"
#include "utils/filesystemUtils.hpp"
#include "utils/htmlUtils.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>

namespace {

inline std::string makePartDelimiter(const std::string& boundary) {
    return "--" + boundary + "\r\n";
}

inline std::string makeCloseDelimiter(const std::string& boundary) {
    return "--" + boundary + "--";
}

/**
 * Find the next occurrence of `delimiter` in `body` at or after `startPos`.
 * Returns true if found (and sets outPos), false otherwise.
 */
bool findNextDelimiter(const std::string& body, const std::string& delimiter, size_t startPos,
                       size_t& outPos) {
    outPos = body.find(delimiter, startPos);
    return outPos != std::string::npos;
}

/**
 * Check if at position `pos` in `body` there is a closing boundary.
 */
bool isCloseDelimiter(const std::string& body, size_t pos, const std::string& closeDelimiter) {
    return body.compare(pos, closeDelimiter.size(), closeDelimiter) == 0;
}

/**
 * Given a single “part” (everything between delimiters), split it into
 * headersBlock (before the first "\r\n\r\n") and dataBlock (after it).
 * Returns false if no double‐CRLF is found.
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
 * Look for filename="..." inside headersBlock. If found, extract the filename
 * (without the quotes) into `outFilename` and return true. Otherwise return false.
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
 * If `data` ends with "\r\n", strip those two characters off. Otherwise return as-is.
 */
std::string trimEndingCRLF(const std::string& data) {
    if (data.size() >= 2 && data[data.size() - 2] == '\r' && data[data.size() - 1] == '\n') {
        return data.substr(0, data.size() - 2);
    }
    return data;
}

/**
 * Parse a multipart/form-data body and extract the first file‐part’s
 * filename and its raw content. Returns true on success, false otherwise.
 *
 * - body:        full HTTP request body (including all parts)
 * - boundary:    the “boundary” token from Content-Type, e.g. "----WebKitFormBoundary…"
 * - filename:    output parameter; set to the file’s original name
 * - fileContent: output parameter; set to the file’s bytes (as a std::string)
 *
 * This will scan part by part until it finds a “filename=” header. If none
 * is found, it ultimately returns false.
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

HttpResponse handleMultipartForm(const HttpRequest& request, const Server& server,
                                 const std::string& fullDirPath) {
    std::string contentType = request.getHeader("Content-Type");
    size_t      bpos        = contentType.find("boundary=");
    if (bpos == std::string::npos) {
        Logger::logFrom(LogLevel::WARN, "handleMultipartForm", "Missing boundary in Content-Type");
        return ResponseBuilder::generateError(400, server, request);
    }
    std::string boundary = contentType.substr(bpos + 9);

    std::string extractedFilename;
    std::string fileContent;
    if (!parseMultipartForm(request.getBody(), boundary, extractedFilename, fileContent)) {
        Logger::logFrom(LogLevel::WARN, "handleMultipartForm", "Failed to parse multipart form");
        return ResponseBuilder::generateError(400, server, request);
    }

    if (extractedFilename.empty()) {
        extractedFilename = "upload_" + std::to_string(std::time(nullptr));
    }

    std::string fullpath = joinPath(fullDirPath, extractedFilename);

    std::ofstream out(fullpath, std::ios::binary);
    if (!out.is_open()) {
        Logger::logFrom(LogLevel::ERROR, "handleMultipartForm",
                        "Failed to open file for writing: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    out << fileContent;
    out.close();
    if (out.fail()) {
        Logger::logFrom(LogLevel::ERROR, "handleMultipartForm",
                        "Failed to write file content to: " + fullpath);
        return ResponseBuilder::generateError(500, server, request);
    }

    Logger::logFrom(LogLevel::INFO, "handleMultipartForm", "Successfully uploaded: " + fullpath);
    return ResponseBuilder::generateSuccess(
        201, "<html><body><h1>Uploaded: " + htmlEscape(extractedFilename) + "</h1></body></html>",
        "text/html", request);
}
