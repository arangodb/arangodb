#!/usr/bin/env python3
# Mint an ArangoDB-compatible HS256 JWT.
# Usage:
#   mkjwt.py superuser <secret-file>          -> {"iss":"arangodb","server_id":"test"}
#   mkjwt.py user <secret-file> <username>    -> {"iss":"arangodb","preferred_username":"<username>"}
import sys, json, hmac, hashlib, base64

def b64u(b: bytes) -> str:
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()

def sign(secret: bytes, header: dict, payload: dict) -> str:
    h = b64u(json.dumps(header, separators=(",", ":")).encode())
    p = b64u(json.dumps(payload, separators=(",", ":")).encode())
    signing_input = f"{h}.{p}".encode()
    sig = hmac.new(secret, signing_input, hashlib.sha256).digest()
    return f"{h}.{p}.{b64u(sig)}"

mode = sys.argv[1]
secret = open(sys.argv[2], "rb").read()
header = {"alg": "HS256", "typ": "JWT"}
if mode == "superuser":
    payload = {"iss": "arangodb", "server_id": "test"}
elif mode == "user":
    payload = {"iss": "arangodb", "preferred_username": sys.argv[3]}
else:
    sys.exit("unknown mode")
print(sign(secret, header, payload))
