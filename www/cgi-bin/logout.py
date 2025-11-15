#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os

# Cookie에서 session_id 읽기
cookies = os.environ.get('HTTP_COOKIE', '')
session_id = None

for cookie in cookies.split(';'):
    cookie = cookie.strip()
    if cookie.startswith('session_id='):
        session_id = cookie.split('=', 1)[1]
        break

# 세션 파일 삭제
if session_id:
    session_file = "/tmp/webserv_sessions/" + session_id + ".session"
    if os.path.exists(session_file):
        try:
            os.remove(session_file)
        except:
            pass

# 쿠키 삭제 (만료 시간을 과거로 설정)
print("Set-Cookie: session_id=; Path=/; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT")
print("Content-Type: text/html\n")
print("<html><body>")
print("<h1>Logged Out</h1>")
print("<p>You have been successfully logged out.</p>")
print("<a href='/welcome.html'>Home</a> | ")
print("<a href='/login.html'>Login Again</a>")
print("</body></html>")
