#include "http/StatusCode.hpp"

std::string StatusCode::getReasonPhrase(int code) {
	switch (code) {
		// 2xx Success
		case 200: return "OK";
		case 201: return "Created";
		case 202: return "Accepted";
		case 204: return "No Content";

		// 3xx Redirection
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 304: return "Not Modified";

		// 4xx Client Error
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 408: return "Request Timeout";
		case 409: return "Conflict";
		case 410: return "Gone";
		case 413: return "Payload Too Large";
		case 414: return "URI Too Long";
		case 431: return "Request Header Fields Too Large";  // ← 추가!

		// 5xx Server Error
		case 500: return "Internal Server Error";
		case 501: return "Not Implemented";
		case 502: return "Bad Gateway";
		case 503: return "Service Unavailable";
		case 504: return "Gateway Timeout";
		case 505: return "HTTP Version Not Supported";

		default: return "Unknown Status";
	}
}

std::string StatusCode::getErrorDescription(int code) {
	switch (code) {
		case 400:
			return "The request syntax is invalid.";
		case 401:
			return "Authentication is required.";
		case 403:
			return "You do not have permission to access this resource.";
		case 404:
			return "The requested resource was not found.";
		case 405:
			return "The HTTP method is not allowed for this resource.";
		case 408:
			return "The server timed out waiting for the request.";
		case 409:
			return "The request conflicts with the current state.";
		case 410:
			return "The requested resource is no longer available.";
		case 413:
			return "The request body is too large.";
		case 414:
			return "The request URI is too long.";
		case 431:
			return "The request header fields are too large.";  // ← 추가!
		case 500:
			return "An internal server error occurred.";
		case 501:
			return "The server does not support the requested functionality.";
		case 502:
			return "The server received an invalid response from an upstream server.";
		case 503:
			return "The server is temporarily unavailable.";
		case 504:
			return "The gateway timed out.";
		case 505:
			return "The HTTP version is not supported.";
		default:
			return "An error occurred.";
	}
}

bool StatusCode::isValidStatusCode(int code) {
	switch (code) {
		case 200: case 201: case 202: case 204:
		case 301: case 302: case 304:
		case 400: case 401: case 403: case 404: case 405:
		case 408: case 409: case 410: case 413: case 414: case 431:
		case 500: case 501: case 502: case 503: case 504: case 505:
			return true;
		default:
			return false;
	}
}

bool StatusCode::isSuccessStatus(int code) {
	return code >= 200 && code < 300;
}

bool StatusCode::isClientError(int code) {
	return code >= 400 && code < 500;
}

bool StatusCode::isServerError(int code) {
	return code >= 500 && code < 600;
}
