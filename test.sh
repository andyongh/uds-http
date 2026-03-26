#!/bin/bash

# Start server
echo "[*] Starting ./uds_server in background..."
./uds_server &
SERVER_PID=$!
sleep 1

SOCKET="/tmp/http.sock"

echo -e "\n[*] 1. Test missing Authorization header"
curl -s -w "\nHTTP Status: %{http_code}\n" --unix-socket "$SOCKET" http://localhost/

echo -e "\n[*] 2. Test invalid token"
curl -s -w "\nHTTP Status: %{http_code}\n" -H "Authorization: Bearer wrongtoken" --unix-socket "$SOCKET" http://localhost/

echo -e "\n[*] 3. Test valid token with JSON payload"
curl -s -w "\nHTTP Status: %{http_code}\n" -H "Authorization: Bearer secret123" -H "Content-Type: application/json" -d '{"hello": "world"}' --unix-socket "$SOCKET" http://localhost/

echo -e "\n[*] 4. Test missing JSON / invalid JSON payload"
curl -s -w "\nHTTP Status: %{http_code}\n" -H "Authorization: Bearer secret123" -d 'not json at all' --unix-socket "$SOCKET" http://localhost/

echo -e "\n[*] 5. Test Accept-Encoding: deflate"
curl -s -w "\nHTTP Status: %{http_code}\n" -H "Authorization: Bearer secret123" -H "Accept-Encoding: deflate" -d '{"msg":"compress this"}' --unix-socket "$SOCKET" http://localhost/ | tee raw_deflate.out | wc -c
echo "Bytes stored in raw_deflate.out"

echo -e "\n[*] Stopping Server..."
kill -9 $SERVER_PID
