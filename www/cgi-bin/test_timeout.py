#!/usr/bin/env python3

import time

# CGI 타임아웃 테스트 스크립트
# 5초 이상 무한루프를 돌아서 504 Gateway Timeout을 유발

print("Content-Type: text/plain")
print()

# 무한루프: 타임아웃될 때까지 계속 대기
while True:
    time.sleep(1)
