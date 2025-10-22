#!/usr/bin/python3
# CGI file upload handler with multipart/form-data support

import os
import sys
import cgi
import cgitb

# Enable detailed error messages (for debugging)
cgitb.enable()

print("Content-Type: text/html")
print("Status: 200 OK")
print()

print("<html>")
print("<head><title>File Upload Result</title></head>")
print("<body>")
print("<h1>CGI File Upload Handler</h1>")

# Parse multipart form data from stdin
form = cgi.FieldStorage()

# Display environment variables for debugging
request_method = os.environ.get('REQUEST_METHOD', '')
content_type = os.environ.get('CONTENT_TYPE', '')
content_length = os.environ.get('CONTENT_LENGTH', '0')

print("<h2>Request Info:</h2>")
print(f"<p><b>Method:</b> {request_method}</p>")
print(f"<p><b>Content-Type:</b> {content_type}</p>")
print(f"<p><b>Content-Length:</b> {content_length}</p>")

# Process uploaded files
if "file" in form:
    fileitem = form["file"]

    if fileitem.filename:
        # Extract filename
        filename = os.path.basename(fileitem.filename)

        # Upload directory (adjust as needed)
        upload_dir = "/tmp/uploads"

        # Create directory if it doesn't exist
        if not os.path.exists(upload_dir):
            os.makedirs(upload_dir)

        # Save file
        filepath = os.path.join(upload_dir, filename)

        try:
            with open(filepath, 'wb') as f:
                f.write(fileitem.file.read())

            file_size = os.path.getsize(filepath)

            print("<h2>Upload Success!</h2>")
            print(f"<p><b>Filename:</b> {filename}</p>")
            print(f"<p><b>Saved to:</b> {filepath}</p>")
            print(f"<p><b>File size:</b> {file_size} bytes</p>")

            # Show content type if available
            if hasattr(fileitem, 'type'):
                print(f"<p><b>Content-Type:</b> {fileitem.type}</p>")

        except Exception as e:
            print(f"<h2>Error saving file:</h2>")
            print(f"<p>{str(e)}</p>")
    else:
        print("<p>No filename provided</p>")
else:
    print("<p>No file field in form data</p>")

# Display all form fields
if form.keys():
    print("<h2>All Form Fields:</h2>")
    print("<ul>")
    for key in form.keys():
        value = form.getvalue(key)
        if isinstance(value, str):
            print(f"<li><b>{key}:</b> {value}</li>")
        else:
            print(f"<li><b>{key}:</b> [File data]</li>")
    print("</ul>")

print("</body>")
print("</html>")
