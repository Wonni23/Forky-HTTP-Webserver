#ifndef STATUS_CODE_HPP
# define STATUS_CODE_HPP

#include <string>

class StatusCode {
	public:
	// 2xx Success
	static const int OK                             = 200;
	static const int CREATED                        = 201;
	static const int ACCEPTED                       = 202;
	static const int NO_CONTENT                     = 204;

	// 3xx Redirection
	static const int MOVED_PERMANENTLY              = 301;
	static const int FOUND                          = 302;
	static const int NOT_MODIFIED                   = 304;

	// 4xx Client Error
	static const int BAD_REQUEST                    = 400;
	static const int UNAUTHORIZED                   = 401;
	static const int FORBIDDEN                      = 403;
	static const int NOT_FOUND                      = 404;
	static const int METHOD_NOT_ALLOWED             = 405;
	static const int CONFLICT                       = 409;
	static const int GONE                           = 410;
	static const int REQUEST_TIMEOUT                = 408;
	static const int PAYLOAD_TOO_LARGE              = 413;  // 요청 본문 크기
	static const int URI_TOO_LONG                   = 414;  // URL 길이
	static const int REQUEST_HEADER_FIELDS_TOO_LARGE = 431; // ← 추가!

	// 5xx Server Error
	static const int INTERNAL_SERVER_ERROR          = 500;
	static const int NOT_IMPLEMENTED                = 501;
	static const int BAD_GATEWAY                    = 502;
	static const int SERVICE_UNAVAILABLE            = 503;
	static const int GATEWAY_TIMEOUT                = 504;
	static const int HTTP_VERSION_NOT_SUPPORTED     = 505;

	// 메서드들
	static std::string  getReasonPhrase(int code);
	static std::string  getErrorDescription(int code);
	static bool         isValidStatusCode(int code);
	static bool         isSuccessStatus(int code);
	static bool         isClientError(int code);
	static bool         isServerError(int code);
};


#endif