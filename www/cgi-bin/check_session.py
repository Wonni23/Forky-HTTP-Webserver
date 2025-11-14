#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import time
import json

# Cookie에서 session_id 읽기
cookies = os.environ.get('HTTP_COOKIE', '')
session_id = None

for cookie in cookies.split(';'):
    cookie = cookie.strip()
    if cookie.startswith('session_id='):
        session_id = cookie.split('=', 1)[1]
        break

response = {
    "valid": False,
    "username": "",
    "message": ""
}

if not session_id:
    response["message"] = "No session cookie found"
else:
    session_file = "/tmp/webserv_sessions/" + session_id + ".session"

    if not os.path.exists(session_file):
        response["message"] = "Session file not found"
    else:
        # 세션 만료 확인
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
            response["message"] = "Session expired"
            os.remove(session_file)
        else:
            response["valid"] = True
            response["username"] = username
            response["message"] = "Session valid"

# JSON 응답
print("Content-Type: application/json\n")
print(json.dumps(response))
