#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
import subprocess

symbolizer = subprocess.Popen(
    ["llvm-symbolizer-16", "--demangle", "--inlines", "--default-arch=x86_64"],
    stdout=subprocess.PIPE,
    stdin=subprocess.PIPE,
)


def symbolize(line):
    symbolizer.stdin.write(line)
    symbolizer.stdin.flush()
    result = ""
    while True:
        line = symbolizer.stdout.readline().decode("utf8")
        result += line
        if line == "\n":
            break
    return result


class Server(BaseHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers["Content-Length"])
        body = self.rfile.read(content_length)
        print(body)
        resp = symbolize(body)
        print(resp)
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write(resp.encode("utf8"))


def run():
    port = 43210
    server_address = ("localhost", port)
    httpd = HTTPServer(server_address, Server)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    httpd.server_close()


if __name__ == "__main__":
    run()
