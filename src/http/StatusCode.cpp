#include "http/StatusCode.hpp"

std::string StatusCode::getReasonPhrase(int code) {
	switch (code) {
		case OK: return "OK";
		case CREATED: return "Created";
		case NO_CONTENT: return "No Content";
		
		case MOVED_PERMANENTLY: return "Moved Permanently";
		case FOUND: return "Found";
		case NOT_MODIFIED: return "Not Modified";
		
		case BAD_REQUEST: return "Bad Request";
		case UNAUTHORIZED: return "Unauthorized";
		case FORBIDDEN: return "Forbidden";
		case NOT_FOUND: return "Not Found";
		case METHOD_NOT_ALLOWED: return "Method Not Allowed";
		case REQUEST_TIMEOUT: return "Request Timeout";
		case PAYLOAD_TOO_LARGE: return "Payload Too Large";
		case URI_TOO_LONG: return "URI Too Long";
		
		case INTERNAL_SERVER_ERROR: return "Internal Server Error";
		case NOT_IMPLEMENTED: return "Not Implemented";
		case BAD_GATEWAY: return "Bad Gateway";
		case SERVICE_UNAVAILABLE: return "Service Unavailable";
		case GATEWAY_TIMEOUT: return "Gateway Timeout";
		
		default: return "Unknown Status";
	}
}

std::string StatusCode::getErrorDescription(int code) {
	switch (code) {
		case BAD_REQUEST:
			return "The server could not understand the request due to invalid syntax.";
		case UNAUTHORIZED:
			return "Authentication is required to access this resource.";
		case FORBIDDEN:
			return "You don't have permission to access this resource.";
		case NOT_FOUND:
			return "The requested resource could not be found on this server.";
		case METHOD_NOT_ALLOWED:
			return "The request method is not allowed for this resource.";
		case PAYLOAD_TOO_LARGE:
			return "The request payload is too large.";
		case INTERNAL_SERVER_ERROR:
			return "The server encountered an unexpected condition.";
		default:
			return "An error occurred while processing your request.";
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

bool StatusCode::isValidStatusCode(int code) {
	return code >= 100 && code <= 599;
}
