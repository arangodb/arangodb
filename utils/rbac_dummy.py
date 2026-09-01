#!/usr/bin/python3

import argparse
from functools import cached_property
from http.cookies import SimpleCookie
from http.server import HTTPServer, SimpleHTTPRequestHandler, BaseHTTPRequestHandler
from urllib.parse import parse_qsl, urlparse
from threading import Thread, Lock
from multiprocessing import Process
import json
import jwt

T_SECRET = "Open Sesame!Open Sesame!Open Ses"

class StoppableHTTPServer(HTTPServer):
    def run(self):
        try:
            self.serve_forever()
        except KeyboardInterrupt:
            pass
        finally:
            # Clean-up server (close socket, etc.)
            self.server_close()

class WebRequestHandler(BaseHTTPRequestHandler):
    @cached_property
    def url(self):
        return urlparse(self.path)
# STFU we don't care.
    def log_message(self, format, *args):
        pass
    @cached_property
    def query_data(self):
        return dict(parse_qsl(self.url.query))

    @cached_property
    def post_data(self):
        content_length = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(content_length)

#    def do_POST(self):
#        global T_SECRET
#        if self.path != "/_integration/authorization/v1/evaluate-token-many":
#            self.send_response(500)
#            self.send_header("Content-Type", "application/json")
#            self.end_headers()
#            self.wfile.write(self.get_response().encode("utf-8"))
#            return
#        postbody = self.rfile.read(int(self.headers["Content-Length"]))
#        #jbody = json.loads(postbody)
#        #print('-----')
#        #parsed_token = jwt.decode(jbody['token'], T_SECRET, algorithms=["HS256"])
#        #print(parsed_token)
#        #del(jbody['token'])
#        # print(jbody)
#
#        response_data = self.get_response().encode("utf-8")
#        self.send_response(200)
#        self.send_header("Connection", "keep-alive")
#        self.send_header("Content-Length", str(len(response_data)))
#        self.send_header("Content-Type", "application/json")
#        self.end_headers()
#        self.wfile.write(response_data)
    def do_POST(self):
        global T_SECRET
        if self.path != "/_integration/authorization/v1/evaluate-token-many":
            self.send_response(500)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(self.get_response().encode("utf-8"))
            return
        postbody = self.rfile.read(int(self.headers["Content-Length"]))
        jbody = json.loads(postbody)
        #print('-----')
        parsed_token = jwt.decode(jbody['token'], T_SECRET, algorithms=["HS256"])
        #print(parsed_token)
        del(jbody['token'])
        print(jbody)

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(self.get_response().encode("utf-8"))

    def get_response(self):
        return json.dumps(
            {
                "effect": "Allow",
                "message": "Access Granted",
                "items": [
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    { "effect": "Allow", "message": "" },
                    # { "effect": "Deny",  "message": "denied" }
                ]
            }
        )

def main(args):
    global T_SECRET
    T_SECRET = args.jwtstr
    server = HTTPServer((args.host, args.port), WebRequestHandler)
    server.serve_forever()

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", type=str, default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--jwtstr", type=str, default="Open Sesame!Open Sesame!Open Ses")
    return parser.parse_args()

if __name__ == "__main__":
    main(parse_args())

RBAC_THREAD_LOCK = Lock()
RBAC_SERVER = None
RBAC_THREAD = False
RBAC_THREAD = None

def spawn_rbac_dummy_thread(jwt_key, port):
    """launch the RBAC dummy thread"""
    global RBAC_THREAD, RBAC_SERVER, T_SECRET
    T_SECRET = jwt_key
    print("starting RBAC dummy thread")
    RBAC_SERVER = StoppableHTTPServer(("0.0.0.0", port), WebRequestHandler)
    RBAC_THREAD = Process(target=RBAC_SERVER.run)
    RBAC_THREAD.start()


def shutdown_rbac_dummy_thread():
    """terminate the RBAC dummy thread"""
    global RBAC_THREAD, RBAC_SERVER
    if RBAC_THREAD is not None:
        print("stopping RBAC dummy thread")
        # RBAC_SERVER.shutdown()
        RBAC_THREAD.terminate()
        RBAC_THREAD.join()

#def spawn_rbac_dummy_thread(jwt_key, port):
#    """launch the RBAC dummy thread"""
#    global RBAC_THREAD, RBAC_SERVER, T_SECRET
#    T_SECRET = jwt_key
#    print("starting RBAC dummy thread")
#    RBAC_SERVER = StoppableHTTPServer(("0.0.0.0", port), WebRequestHandler)
#    RBAC_THREAD = Thread(None, RBAC_SERVER.run)
#    RBAC_THREAD.start()
#
#
#def shutdown_rbac_dummy_thread():
#    """terminate the RBAC dummy thread"""
#    global RBAC_THREAD, RBAC_SERVER
#    if RBAC_THREAD is not None:
#        print("stopping RBAC dummy thread")
#        RBAC_SERVER.shutdown()
#        RBAC_THREAD.join()
