#!/usr/bin/python3
# CGI test - GET method with QUERY_STRING

import os
import cgi

print("Content-Type: text/html")
print("Status: 200 OK")
print()

query_string = os.environ.get('QUERY_STRING', '')
request_method = os.environ.get('REQUEST_METHOD', '')

print("<html>")
print("<head><title>GET Test</title></head>")
print("<body>")
print("<h1>GET Request Test</h1>")
print("<p><b>Request Method:</b> {}</p>".format(request_method))
print("<p><b>Query String:</b> {}</p>".format(query_string))

if query_string:
    # Parse query string
    form = cgi.FieldStorage()
    print("<h2>Parsed Parameters:</h2>")
    print("<ul>")
    for key in form.keys():
        value = form.getvalue(key)
        print("<li><b>{}:</b> {}</li>".format(key, value))
    print("</ul>")
else:
    print("<p>No query parameters provided.</p>")
    print("<p>Try: <a href='?name=test&value=123'>?name=test&value=123</a></p>")

print("</body>")
print("</html>")
