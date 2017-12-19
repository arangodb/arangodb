# Uploading binary data to remote locations from Foxx

## Problem

My Foxx should handle binary data and upload it to another service.

## Solution
ArangoDB can handle binary data using the buffer class. However, these don't mix well with strings. Therefore we have to construct the mime document by hand using buffers. We will choose the buffer to be loaded from a binary file. [http bin](http://httpbin.org) will help us to revalidate all we did along the lines of the RFC.

```js
var request = require("org/arangodb/request")
var fs = require("fs")
var fileBuffer = fs.readBuffer("resources/Icons/arangodb.ico");
var mimeBoundary = "1234MyMimeBoundary";
var endpoint = "http://httpbin.org/post";
var bodyStart = '--' + mimeBoundary + '\r\n'+
    'Content-Disposition: form-data; name="upload"; filename="arangodb.ico"\r\n'+
    'Content-Type: application/octet-stream\r\n\r\n';
var bodyEnd = '\r\n--' + mimeBoundary + '--\r\n';

var multipartBody = Buffer.concat([
    new Buffer(bodyStart),
    fileBuffer,
    new Buffer(bodyEnd)
]);

var opts = {
    url: endpoint,
    method: "POST",
    headers: {
        "Content-Type" : "multipart/form-data; boundary=" + mimeBoundary
    },
    body: multipartBody
};
var resp = request(opts);
console.log(resp);
```

Since we really want to know whats going on on the wire, we'll use [sniffing](http://www.tcpdump.org/). You can use the nice user interface program [Wireshark](http://wireshark.org). We will use the simple [ngrep](http://ngrep.sourceforge.net/) to output the traffic on the console to us:

```
ngrep -Wbyline host httpbin.org > /tmp/log
interface: eth0 (192.168.1.0/255.255.255.0)
filter: (ip or ip6) and ( host httpbin.org )
####
T 192.168.1.2:45487 -> 54.175.219.8:80 [A]
POST /post HTTP/1.1
Host: httpbin.org
Connection: Keep-Alive
User-Agent: ArangoDB
Accept-Encoding: deflate
content-type: multipart/form-data; boundary=1234MyMimeBoundary
Content-Length: 205020

--1234MyMimeBoundary
Content-Disposition: form-data; name="upload"; filename="test.png"
Content-Type: application/octet-stream

--1234MyMimeBoundary--

####
T 54.175.219.8:80 -> 192.168.1.2:45487 [A]
HTTP/1.1 200 OK.
Server: nginx.
Date: Mon, 23 Mar 2015 14:25:52 GMT.
Content-Type: application/json.
Content-Length: 273575.
Connection: keep-alive.
Access-Control-Allow-Origin: *.
Access-Control-Allow-Credentials: true.
.
{
  "args": {},
  "data": "",
  "files": {
    "upload": "data:application/octet-stream;base64,AA.....///w=="
  },
  "form": {},
  "headers": {
    "Accept-Encoding": "deflate",
    "Content-Length": "205020",
    "Content-Type": "m
##
T 54.175.219.8:80 -> 192.168.1.2:45487 [AP]
ultipart/form-data; boundary=1234MyMimeBoundary",
    "Host": "httpbin.org",
    "User-Agent": "ArangoDB"
  },
  "json": null,
  "origin": "80.152.136.67",
  "url": "http://httpbin.org/post"
}

####exit
592 received, 0 dropped
```

Here we see the HTTP request in all its beauty - everything went well - our mime implementation is proven to work.

**Author:** [Wilfried Goesgens](https://github.com/dothebart)

**Tags:**  #foxx
