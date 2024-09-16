#!/usr/bin/env python3

import urllib.request
import sys
import os

def run():
    if 'HOST_SYMBOLIZER_URL' in os.environ:
        url = os.environ['HOST_SYMBOLIZER_URL']
    else:
        url = "http://localhost:43210"
    for line in sys.stdin:
        data = line.encode("utf8")
        headers = {"content-type": "text/plain"}
        req = urllib.request.Request(url, data=data, headers=headers)
        response = urllib.request.urlopen(req)
        sys.stdout.write(response.read().decode("utf8"))
        sys.stdout.flush()


if __name__ == "__main__":
    run()
