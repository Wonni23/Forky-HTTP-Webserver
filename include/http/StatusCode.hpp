#ifndef STATUS_CODE_HPP
# define STATUS_CODE_HPP

#include <string>

class	StatusCode {
	public:
	// HTTP status code constants
	static const int	OK							= 200;
	static const int	CREATED						= 201;
	static const int	NO_CONTENT					= 204;

	static const int	MOVED_PERMANENTLY			= 301;
	static const int	FOUND						= 302;
	static const int	NOT_MODIFIED				= 304;

	static const int	BAD_REQUEST					= 400;
	static const int	UNAUTHORIZED				= 401;
	static const int	FORBIDDEN					= 402;
	static const int	NOT_FOUND					= 404;
	static const int	METHOD_NOT_ALLOWED			= 405;
	static const int	REQUEST_TIMEOUT				= 408;
	static const int	PAYLOAD_TOO_LARGE			= 413;
	static const int	URI_TOO_LONG				= 414;

	static const int	INTERNAL_SERVER_ERROR		= 500;
	static const int	NOT_IMPLEMENTED				= 501;
	static const int	BAD_GATEWAY					= 502;
	static const int	SERVICE_UNAVAILABLE			= 503;
	static const int	GATEWAY_TIMEOUT				= 504;
	static const int	HTTP_VERSION_NOT_SUPPORTED	= 505;

	static std::string	getReasonPhrase(int code);
	static std::string	getErrorDescription(int code);
	static bool			isValidStatusCode(int code);
	static bool			isSuccessStatus(int code);
	static bool			isClientError(int code);
	static bool			isServerError(int code);
};

#endif