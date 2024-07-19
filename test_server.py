#!/usr/bin/env python3

import http.server

class CorsHttpRequestHandler(http.server.SimpleHTTPRequestHandler):

    def end_headers(self):
        self.send_cors_headers()
        http.server.SimpleHTTPRequestHandler.end_headers(self)

    def send_cors_headers(self):
        '''
        Adds Cross-Origin Resource Sharing (CORS) headers to the HTTP request. These enable browser
        support for SharedArrayBuffer which is used by Emscripten for multithreading. See
        https://emscripten.org/docs/porting/pthreads.html for further details.
        '''
        self.send_response(200)
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser()

    parser.add_argument(
        'port', 
        action='store', 
        default=8000, 
        type=int, 
        nargs='?', 
        help='specify alternate port (default: 8000)')

    args = parser.parse_args()

    http.server.test(
        HandlerClass=CorsHttpRequestHandler,
        ServerClass=http.server.HTTPServer,
        port=args.port)
