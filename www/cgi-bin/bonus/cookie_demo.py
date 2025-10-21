#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys

# 1. 기존 쿠키 읽기 (환경변수 HTTP_COOKIE에서)
cookies = {}
cookie_header = os.environ.get('HTTP_COOKIE', '')
if cookie_header:
    for cookie in cookie_header.split(';'):
        cookie = cookie.strip()
        if '=' in cookie:
            name, value = cookie.split('=', 1)
            cookies[name] = value

# 2. 요청 확인
query_string = os.environ.get('QUERY_STRING', '')

# 3. 액션 처리
if 'action=set' in query_string:
    # 쿠키 설정
    print("Set-Cookie: demo_cookie=HelloWorld; Path=/; Max-Age=3600; HttpOnly")
    print("Set-Cookie: user_pref=darkmode; Path=/; Max-Age=7200")
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Cookies Set!</h1>")
    print("<p>Two cookies have been set:</p>")
    print("<ul>")
    print("<li><code>demo_cookie=HelloWorld</code> (expires in 1 hour)</li>")
    print("<li><code>user_pref=darkmode</code> (expires in 2 hours)</li>")
    print("</ul>")
    print("<a href='/cgi-bin/bonus/cookie_demo.py'>Check Cookies</a>")
    print("</body></html>")

elif 'action=delete' in query_string:
    # 쿠키 삭제
    print("Set-Cookie: demo_cookie=; Path=/; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT")
    print("Set-Cookie: user_pref=; Path=/; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT")
    print("Content-Type: text/html\n")
    print("<html><body>")
    print("<h1>Cookies Deleted!</h1>")
    print("<p>All cookies have been removed.</p>")
    print("<a href='/cgi-bin/bonus/cookie_demo.py'>Go Back</a>")
    print("</body></html>")

else:
    # 쿠키 표시
    print("Content-Type: text/html\n")
    print("<html>")
    print("<head>")
    print("<style>")
    print("body { font-family: Arial; max-width: 600px; margin: 50px auto; }")
    print(".cookie-box { background: #f0f0f0; padding: 15px; border-radius: 5px; margin: 10px 0; }")
    print(".button { display: inline-block; padding: 10px 20px; background: #007bff; color: white; text-decoration: none; border-radius: 4px; margin: 5px; }")
    print("</style>")
    print("</head>")
    print("<body>")
    print("<h1>Cookie Demo (Light Version)</h1>")

    if cookies:
        print("<h2>Current Cookies:</h2>")
        for name, value in cookies.items():
            print(f"<div class='cookie-box'><strong>{name}</strong> = <code>{value}</code></div>")
    else:
        print("<p>No cookies found. Click 'Set Cookies' to create some.</p>")

    print("<hr>")
    print("<a href='/cgi-bin/bonus/cookie_demo.py?action=set' class='button'>Set Cookies</a>")
    print("<a href='/cgi-bin/bonus/cookie_demo.py?action=delete' class='button'>Delete Cookies</a>")
    print("<a href='/html/bonus/cookie_test.html' class='button'>Back to Home</a>")
    print("</body>")
    print("</html>")
