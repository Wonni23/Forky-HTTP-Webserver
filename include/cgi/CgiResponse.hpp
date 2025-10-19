#ifndef CGI_RESPONSE_HPP
#define CGI_RESPONSE_HPP

#include <string>
#include "http/HttpResponse.hpp"
#include "dto/ConfigDTO.hpp"

/**
 * @brief Parses CGI script output and converts it to HttpResponse object.
 *
 * Standard CGI output format:
 * Content-Type: text/html\r\n
 * Status: 200 OK\r\n
 * \r\n
 * <html>...</html>
 */
class CgiResponseParser {
public:
	/**
	 * @brief Parse CGI output and create HttpResponse object
	 *
	 * @param cgiOutput CGI script stdout output (headers + body)
	 * @return HttpResponse object pointer. Returns NULL on parsing failure.
	 */
	HttpResponse* parse(const std::string& cgiOutput);

private:
	/**
	 * @brief Parse header line to extract name and value
	 * @param line "Content-Type: text/html"
	 * @param name Output parameter - header name
	 * @param value Output parameter - header value
	 * @return true on success
	 */
	bool parseHeaderLine(const std::string& line, std::string& name, std::string& value);

	/**
	 * @brief Trim whitespace from both ends of string
	 */
	std::string trim(const std::string& str);
};

#endif
