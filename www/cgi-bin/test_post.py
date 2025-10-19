#!/usr/bin/python3
# CGI test - POST method with body

import os
import sys
import cgi

print("Content-Type: text/html")
print("Status: 200 OK")
print()

request_method = os.environ.get('REQUEST_METHOD', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')
content_type = os.environ.get('CONTENT_TYPE', '')

print("<html>")
print("<head><title>POST Test</title></head>")
print("<body>")
print("<h1>POST Request Test</h1>")
print("<p><b>Request Method:</b> {}</p>".format(request_method))
print("<p><b>Content-Length:</b> {}</p>".format(content_length))
print("<p><b>Content-Type:</b> {}</p>".format(content_type))

# Read POST body from stdin
if int(content_length) > 0:
    body = sys.stdin.read(int(content_length))
    print("<h2>POST Body:</h2>")
    print("<pre>{}</pre>".format(body))

    # Try to parse as form data
    if 'application/x-www-form-urlencoded' in content_type:
        form = cgi.FieldStorage()
        if form.keys():
            print("<h2>Parsed Form Data:</h2>")
            print("<ul>")
            for key in form.keys():
                value = form.getvalue(key)
                print("<li><b>{}:</b> {}</li>".format(key, value))
            print("</ul>")
else:
    print("<p>No POST body received.</p>")

print("</body>")
print("</html>")
