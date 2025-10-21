#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import cgi

# POST 데이터 읽기
content_length = int(os.environ.get('CONTENT_LENGTH', 0))

if content_length == 0:
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: No data received</h1>")
    print("<a href='/html/bonus/signup.html'>Try Again</a>")
    print("</body></html>")
    sys.exit(0)

post_data = sys.stdin.read(content_length)

# 폼 데이터 파싱 (간단한 파싱: "username=foo&password=bar")
params = {}
for pair in post_data.split('&'):
    if '=' in pair:
        key, value = pair.split('=', 1)
        # URL 디코딩 (간단 버전)
        value = value.replace('+', ' ')
        params[key] = value

username = params.get('username', '')
password = params.get('password', '')

if not username or not password:
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: Username and password required</h1>")
    print("<a href='/html/bonus/signup.html'>Try Again</a>")
    print("</body></html>")
    sys.exit(0)

# 사용자 파일 생성
user_dir = "/tmp/webserv_users/"
user_file = user_dir + username + ".user"

# 디렉토리가 없으면 생성
if not os.path.exists(user_dir):
    os.makedirs(user_dir, mode=0o755)

# 이미 존재하는 사용자인지 확인
if os.path.exists(user_file):
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: User already exists</h1>")
    print("<a href='/html/bonus/signup.html'>Try Again</a>")
    print("</body></html>")
    sys.exit(0)

# 사용자 파일 저장
try:
    with open(user_file, 'w') as f:
        f.write("username=" + username + "\n")
        f.write("password=" + password + "\n")

    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Signup Successful!</h1>")
    print("<p>Welcome, " + username + "!</p>")
    print("<a href='/html/bonus/login.html'>Go to Login</a>")
    print("</body></html>")

except Exception as e:
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Error: Failed to create user</h1>")
    print("<p>" + str(e) + "</p>")
    print("<a href='/html/bonus/signup.html'>Try Again</a>")
    print("</body></html>")
