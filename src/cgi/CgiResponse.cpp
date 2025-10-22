#include "cgi/CgiResponse.hpp"
#include "http/HttpResponse.hpp"
#include "http/StatusCode.hpp"
#include <sstream>
#include <cctype>

HttpResponse* CgiResponseParser::parse(const std::string& cgiOutput) {
	if (cgiOutput.empty()) {
		return NULL; // Empty output
	}

	// 1. Separate headers and body (\r\n\r\n or \n\n)
	size_t headerEndPos = cgiOutput.find("\r\n\r\n");
	std::string headerDelim = "\r\n";

	if (headerEndPos == std::string::npos) {
		// If \r\n\r\n not found, try \n\n
		headerEndPos = cgiOutput.find("\n\n");
		headerDelim = "\n";

		if (headerEndPos == std::string::npos) {
			// No header delimiter found -> parsing failure
			return NULL;
		}
	}

	std::string headersPart = cgiOutput.substr(0, headerEndPos);
	std::string bodyPart = cgiOutput.substr(headerEndPos + headerDelim.length() * 2);

	// 2. Create HttpResponse object
	HttpResponse* response = new HttpResponse();

	// 3. Parse headers line by line
	std::istringstream headerStream(headersPart);
	std::string line;
	int statusCode = StatusCode::OK; // Default value
	std::string contentType;

	while (std::getline(headerStream, line)) {
		// Remove \r character
		if (!line.empty() && line[line.length() - 1] == '\r') {
			line = line.substr(0, line.length() - 1);
		}

		if (line.empty()) {
			continue; // Skip empty lines
		}

		std::string name, value;
		if (!parseHeaderLine(line, name, value)) {
			continue; // Skip lines that failed to parse
		}

		// Handle standard CGI headers
		if (name == "Content-Type" || name == "content-type") {
			contentType = value;
		} else if (name == "Status" || name == "status") {
			// Status: 200 OK format
			std::istringstream statusStream(value);
			statusStream >> statusCode;
		} else {
			// Add other headers as-is
			response->setHeader(name, value);
		}
	}

	// 4. Configure HttpResponse
	response->setStatus(statusCode);
	response->setBody(bodyPart);

	// Set Content-Type if present
	if (!contentType.empty()) {
		response->setContentType(contentType);
	} else {
		// Default value
		response->setContentType("text/html; charset=utf-8");
	}

	return response;
}

bool CgiResponseParser::parseHeaderLine(const std::string& line, std::string& name, std::string& value) {
	size_t colonPos = line.find(':');
	if (colonPos == std::string::npos) {
		return false; // No ':' found
	}

	name = trim(line.substr(0, colonPos));
	value = trim(line.substr(colonPos + 1));

	return !name.empty();
}

std::string CgiResponseParser::trim(const std::string& str) {
	size_t start = 0;
	size_t end = str.length();

	// Remove leading whitespace
	while (start < end && std::isspace(str[start])) {
		start++;
	}

	// Remove trailing whitespace
	while (end > start && std::isspace(str[end - 1])) {
		end--;
	}

	return str.substr(start, end - start);
}
