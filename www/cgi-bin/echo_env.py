#!/usr/bin/python3
# CGI test - Print all environment variables

import os

print("Content-Type: text/html")
print("Status: 200 OK")
print()

print("<html>")
print("<head><title>Environment Variables</title></head>")
print("<body>")
print("<h1>CGI Environment Variables</h1>")
print("<table border='1' cellpadding='5'>")
print("<tr><th>Variable</th><th>Value</th></tr>")

# Sort environment variables for easier reading
env_vars = sorted(os.environ.items())
for key, value in env_vars:
    print("<tr><td><b>{}</b></td><td>{}</td></tr>".format(key, value))

print("</table>")
print("</body>")
print("</html>")
