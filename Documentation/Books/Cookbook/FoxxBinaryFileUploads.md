# Handling Binary HTTP documents in Foxx

Handling binary data in JavaScript applications is a bit
tricky because JavaScript does not provide a data type for
binary data. This post explains how to use binary data in 
JavaScript actions written using ArangoDB's [Foxx](http://docs.arangodb.org/Foxx/README.html). 

<!-- more -->

Strings vs. binary data
=======================

Internally, JavaScript strings are [sequences of 16 bit integer values](http://ecma-international.org/ecma-262/5.1/#sec-4.3.16).
Furthermore, the ECMAScript standard requires that a JavaScript
implementation should interpret characters in conformance with the
Unicode standard, using either UCS-2 or UTF-16 encoding. 

While this is fine for handling natural language, it becomes problematic
when trying to work with arbitrary binary data. Binary data cannot be
used safely in a JavaScript string because it may not be valid UTF-16
data.

To make it work anyway, binary data needs to be stored in a wrapper
object. I won't go into details about ES6 typed arrays here, but will
focus on `Buffer` objects.

Binary data in Foxx actions
===========================

A Foxx route that shall handle HTTP POST requests containing arbitrary 
(binary) body in the request body should not use `req.body()`. The 
reason is that `req.body()` will return the body as a JavaScript string,
and this isn't going to work with arbitrary binary data. 

Instead, the `req.rawBodyBuffer()` should be used. This will return the
request body inside a buffer.
Here's an example that stores the received data in a file on the server:

*Foxx action that can handle binary input:*
```js
controller.post('/receive-binary', function (req, res) {
  // fetch request body into the buffer
  var body = req.rawBodyBuffer();
  // create an absolute filename, local to the Foxx application directory
  var filename = applicationContext.foxxFilename("body");

  require("fs").write(filename, body);
});
```

This action can be invoked as follows if the app is mounted with name `app`:

    curl -X POST http://localhost:8529/app/receive-binary --data-binary @filename

This will send the contents of the file `filename` to the server. The Foxx
action will then store the received data as is in a file name `body` in the
application directory.

Returning binary data from a Foxx action is simple, too. Here's a way that
returns the contents of the file named `body` in the application's directory:

*Foxx action that returns contents of a file:*
```js
controller.get('/provide-binary-file', function (req, res) {
  // create an absolute filename, local to the Foxx application directory
  var filename = applicationContext.foxxFilename("body");
  // send the contents, this will also set mime type "application/octet-stream"
  res.sendFile(filename);
});
```
  
It is also possible to return data from an arbitrary buffer:
  
*Foxx action that returns data in a buffer*
```js
controller.get('/provide-binary-buffer', function (req, res) {
  // create an absolute filename, local to the Foxx application directory
  var filename = applicationContext.foxxFilename("body");
  // read the file content into a buffer
  var fileContent = require("fs").readBuffer(filename);

  // TODO: modify the contents of buffer here...

  // send the contents, this will also set mime type "application/octet-stream"
  res.send(fileContent);
});
```

Example application
===================

I quickly put together an example application that shows how to handle arbitrary
binary data in Foxx actions. The example app allows uploading files to the server.
The server will then list these files and allows downloading them again.

The application has no CSS at all. Its only purpose is to demo the server-side code.
The application can be downloaded [here](http://jsteemann.github.io/downloads/code/filelist.zip). An older
version of the application (compatible with 2.3) can be found [here](http://jsteemann.github.io/downloads/code/filelist-app.tar.gz).

Please note that the example application requires ArangoDB 2.3, which is currently
in development.



**Original Author**: [Jan Steemann](http://jsteemann.github.io/blog/2014/10/15/handling-binary-data-in-foxx/)

Tags: #ArangoDB  #Database #Foxx #JavaScript #Binary #Upload

