#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import uuid
import time

# POST 데이터 읽기
content_length = int(os.environ.get('CONTENT_LENGTH', 0))

if content_length == 0:
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: No data received</h1>")
    print("<a href='/html/bonus/login.html'>Try Again</a>")
    print("</body></html>")
    sys.exit(0)

post_data = sys.stdin.read(content_length)

# 폼 데이터 파싱
params = {}
for pair in post_data.split('&'):
    if '=' in pair:
        key, value = pair.split('=', 1)
        value = value.replace('+', ' ')
        params[key] = value

username = params.get('username', '')
password = params.get('password', '')

if not username or not password:
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: Username and password required</h1>")
    print("<a href='/html/bonus/login.html'>Try Again</a>")
    print("</body></html>")
    sys.exit(0)

# 사용자 인증
user_file = "/tmp/webserv_users/" + username + ".user"

if not os.path.exists(user_file):
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: User not found</h1>")
    print("<a href='/html/bonus/login.html'>Try Again</a> | ")
    print("<a href='/html/bonus/signup.html'>Sign Up</a>")
    print("</body></html>")
    sys.exit(0)

# 비밀번호 확인
stored_password = ""
with open(user_file, 'r') as f:
    for line in f:
        if line.startswith("password="):
            stored_password = line[9:].strip()
            break

if stored_password != password:
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: Invalid password</h1>")
    print("<a href='/html/bonus/login.html'>Try Again</a>")
    print("</body></html>")
    sys.exit(0)

# 로그인 성공 - 세션 생성
session_id = str(uuid.uuid4())
session_dir = "/tmp/webserv_sessions/"
session_file = session_dir + session_id + ".session"

# 디렉토리가 없으면 생성
if not os.path.exists(session_dir):
    os.makedirs(session_dir, mode=0o755)

# 세션 파일 저장
now = int(time.time())
with open(session_file, 'w') as f:
    f.write("username=" + username + "\n")
    f.write("created_at=" + str(now) + "\n")
    f.write("last_accessed_at=" + str(now) + "\n")

# Set-Cookie 헤더와 함께 응답
print("Set-Cookie: session_id=" + session_id + "; Path=/; Max-Age=1800; HttpOnly")
print("Content-Type: text/html\n")
print("<html><body>")
print("<h1>Login Successful!</h1>")
print("<p>Welcome back, " + username + "!</p>")
print("<a href='/cgi-bin/bonus/dashboard.py'>Go to Dashboard</a>")
print("</body></html>")
