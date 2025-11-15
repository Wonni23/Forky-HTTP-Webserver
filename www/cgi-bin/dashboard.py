#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import time

# Cookie에서 session_id 읽기
cookies = os.environ.get('HTTP_COOKIE', '')
session_id = None

for cookie in cookies.split(';'):
    cookie = cookie.strip()
    if cookie.startswith('session_id='):
        session_id = cookie.split('=', 1)[1]
        break

# 세션 확인
if not session_id:
    # 세션 없음 - 로그인 페이지로 리디렉션
    print("Status: 302 Found")
    print("Location: /login.html")
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Redirecting to login...</h1>")
    print("</body></html>")
    sys.exit(0)

# 세션 파일 확인
session_file = "/tmp/webserv_sessions/" + session_id + ".session"

if not os.path.exists(session_file):
    # 세션 파일 없음 - 만료되었거나 유효하지 않음
    print("Set-Cookie: session_id=; Path=/; Max-Age=0")
    print("Status: 302 Found")
    print("Location: /login.html")
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Session expired. Redirecting to login...</h1>")
    print("</body></html>")
    sys.exit(0)

# 세션 만료 확인 (30분 = 1800초)
SESSION_TIMEOUT = 1800
now = int(time.time())
last_accessed = 0
username = ""

with open(session_file, 'r') as f:
    for line in f:
        if line.startswith("username="):
            username = line[9:].strip()
        elif line.startswith("last_accessed_at="):
            last_accessed = int(line[17:].strip())

if (now - last_accessed) > SESSION_TIMEOUT:
    # 세션 만료
    os.remove(session_file)
    print("Set-Cookie: session_id=; Path=/; Max-Age=0")
    print("Status: 302 Found")
    print("Location: /login.html")
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Session expired (30 minutes). Redirecting to login...</h1>")
    print("</body></html>")
    sys.exit(0)

# 세션 유효 - last_accessed_at 업데이트
with open(session_file, 'r') as f:
    lines = f.readlines()

with open(session_file, 'w') as f:
    for line in lines:
        if line.startswith("last_accessed_at="):
            f.write("last_accessed_at=" + str(now) + "\n")
        else:
            f.write(line)

# 대시보드 표시
print("Content-Type: text/html\n")
print("<html>")
print("<head><title>Dashboard</title></head>")
print("<body>")
print("<h1>Welcome to Dashboard</h1>")
print("<p>Hello, <strong>" + username + "</strong>!</p>")
print("<p>Session ID: " + session_id + "</p>")
print("<p>Last accessed: " + time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(last_accessed)) + "</p>")
print("<p>Session expires in: " + str(SESSION_TIMEOUT - (now - last_accessed)) + " seconds</p>")
print("<hr>")
print("<a href='/dashboard.py'>Refresh</a> | ")
print("<a href='/logout.py'>Logout</a>")
print("</body>")
print("</html>")
