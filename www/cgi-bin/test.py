#!/usr/bin/python3
# Simple test CGI script

import os

print("Content-Type: text/html")
print("Status: 200 OK")
print()

print("<html>")
print("<head><title>Test CGI</title></head>")
print("<body>")
print("<h1>Test CGI Works!</h1>")
print(f"<p>Request Method: {os.environ.get('REQUEST_METHOD', 'N/A')}</p>")
print(f"<p>Content Length: {os.environ.get('CONTENT_LENGTH', '0')}</p>")
print("</body>")
print("</html>")
