#!/usr/bin/python3
# CGI test - GET and POST combined

import os
import sys
import cgi

print("Content-Type: text/html")
print("Status: 200 OK")
print()

# Get CGI environment variables
request_method = os.environ.get('REQUEST_METHOD', '')
query_string = os.environ.get('QUERY_STRING', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')
content_type = os.environ.get('CONTENT_TYPE', '')

print("<html>")
print("<head><title>CGI Test - GET & POST</title></head>")
print("<body>")
print("<h1>CGI Test - GET and POST</h1>")

# Display request information
print("<h2>Request Information</h2>")
print("<ul>")
print("<li><b>Request Method:</b> {}</li>".format(request_method))
print("<li><b>Query String:</b> {}</li>".format(query_string if query_string else '(empty)'))
print("<li><b>Content-Length:</b> {}</li>".format(content_length))
print("<li><b>Content-Type:</b> {}</li>".format(content_type if content_type else '(empty)'))
print("</ul>")

# Handle GET parameters
if request_method == 'GET':
	print("<h2>GET Parameters</h2>")
	if query_string:
		form = cgi.FieldStorage()
		if form.keys():
			print("<ul>")
			for key in form.keys():
				value = form.getvalue(key)
				print("<li><b>{}:</b> {}</li>".format(key, value))
			print("</ul>")
		else:
			print("<p>Query string present but no parameters parsed.</p>")
	else:
		print("<p>No query parameters provided.</p>")
		print("<p>Try: <a href='?name=test&value=123'>?name=test&value=123</a></p>")

# Handle POST parameters
elif request_method == 'POST':
	print("<h2>POST Request</h2>")

	# Read POST body from stdin
	if int(content_length) > 0:
		body = sys.stdin.read(int(content_length))
		print("<h3>Raw POST Body:</h3>")
		print("<pre>{}</pre>".format(body))

		# Parse form data if it's form-urlencoded
		if 'application/x-www-form-urlencoded' in content_type:
			form = cgi.FieldStorage()
			if form.keys():
				print("<h3>Parsed Form Data:</h3>")
				print("<ul>")
				for key in form.keys():
					value = form.getvalue(key)
					print("<li><b>{}:</b> {}</li>".format(key, value))
				print("</ul>")
			else:
				print("<p>Form data not parsed.</p>")
		elif 'application/json' in content_type:
			print("<p><i>JSON content detected but not automatically parsed.</i></p>")
		elif 'multipart/form-data' in content_type:
			print("<p><i>Multipart form data detected.</i></p>")
		elif 'text/plain' in content_type:
			print("<h3>Text Content:</h3>")
			print("<p>{}</p>".format(body))
		else:
			print("<p><i>Content-Type: {}</i></p>".format(content_type))
	else:
		print("<p>No POST body received.</p>")

else:
	print("<h2>Other Methods</h2>")
	print("<p>Request method: {}</p>".format(request_method))

# Footer with test links
print("<hr>")
print("<h3>Test Links</h3>")
print("<ul>")
print("<li><a href='?test=1&param=hello'>GET Test</a></li>")
print("<li>POST Test: <code>curl -X POST -d 'name=john&age=30' http://localhost:8080/test.py</code></li>")
print("</ul>")

print("</body>")
print("</html>")
