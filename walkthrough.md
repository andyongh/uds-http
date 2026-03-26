# UDS HTTP Server Walkthrough

This document outlines the successful implementation of the high-performance Linux (and macOS!) C HTTP server over a Unix Domain Socket (UDS).

## Final Capabilities

1. **Cross-Platform Event Loop (Valkey [ae](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/ae.c#385-395))**: Natively supports both Linux (`epoll`) and macOS (`kqueue`) using the highly optimized bare metal event loop extracted from Valkey 9.0 ([ae.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/ae.c)), replacing Node.js's `libuv`.
2. **Parsing Efficiency**: Integrated `llhttp` for pipelined HTTP parsing, and [yyjson](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/http_server.c#220-234) for blistering fast JSON parsing and serialization.
3. **Concurrency (<1000)**: The main [accept](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/main.c#21-39) loop pauses after 1000 concurrent active connections.
4. **Drop Bucket Flow Control**: Uses a background [aeCreateTimeEvent](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/ae.c#261-283) algorithm to enforce an RPS limit via a Token Bucket system. Exceeding requests instantly receive `429 Too Many Requests`.
5. **Authentication**: `Authorization` checks using a Bearer token verification hook inside HTTP handler execution.
6. **Compression**: Automatically supports `Accept-Encoding: lz4` and `Accept-Encoding: deflate` via integrated `zlib` and `lz4` libraries.
7. **Keep-Alive**: Fully supports keep-alive connections which was crucial for reaching benchmark maximums, handled by direct POSIX `EAGAIN` read/write buffering strings.
8. **Structured Logging**: Timed monotonic precision logs tracking boot, info, and networking error occurrences.
9. **Observability endpoints (`/metrics`, `/config`)**: Synthesized via [yyjson](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/http_server.c#220-234), exposing active state of total requests, active connections, read/write I/O byte volumes, and RPS capacity parameters.

## Source Code Structure
- **Network Pipeline**: [src/main.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/main.c), [src/http_server.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/http_server.c), [src/http_server.h](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/http_server.h)
- **Event Loop Engine**: [src/ae.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/ae.c), [src/ae_epoll.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/ae_epoll.c), [src/ae_kqueue.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/ae_kqueue.c), [src/config.h](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/config.h), [src/monotonic.h](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/monotonic.h), plus stubs for `zmalloc`.
- **JSON Handler**: [src/json_handler.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/json_handler.c), `src/yyjson.h/c`
- **Algorithm Providers**: [src/auth.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/auth.c)/`h`, [src/flow_control.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/flow_control.c)/`h`, [src/compression.c](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/compression.c)/`h`
- **Libraries Vendored**: `llhttp` (generated `api.c`, `llhttp.c`, `http.c`), [yyjson](file:///Users/andy/Downloads/vibe_coding/uds-http-server/src/http_server.c#220-234), `lz4`.

## Testing

### Functionality Validation
We tested the endpoints successfully via `curl` against [/tmp/http.sock](file:///tmp/http.sock) confirming successful token parsing and structured JSON echo handling via yyjson. Unauthenticated responses correctly trigger `401 Unauthorized` responses. `Accept-Encoding: deflate` correctly streams a zipped buffer payload back.

### Performance Benchmarking
Using a custom Node.js UDS client benchmark script ([benchmark.js](file:///Users/andy/Downloads/vibe_coding/uds-http-server/benchmark.js)) utilizing HTTP keep-alive, we observed sustained peaks:

```
Server listening on Unix Socket: /tmp/http.sock
RPS: 54000
```
This surpasses the threshold requirement of `> 50,000 RPS`.

## Next Steps
To run the server yourself:
```sh
cd /Users/andy/Downloads/vibe_coding/uds-http-server
make
./uds_server
```

You can point tools like `curl --unix-socket /tmp/http.sock ...` towards it!
