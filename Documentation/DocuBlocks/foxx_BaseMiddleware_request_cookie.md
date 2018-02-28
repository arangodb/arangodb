


`request.cookie(name, cfg)`

Read a cookie from the request. Optionally the cookie's signature can be verified.

*Parameter*

* *name*: the name of the cookie to read from the request.
* *cfg* (optional): an object with any of the following properties:
  * *signed* (optional): an object with any of the following properties:
  * *secret*: a secret string that was used to sign the cookie.
  * *algorithm*: hashing algorithm that was used to sign the cookie. Default: *"sha256"*.

If *signed* is a string, it will be used as the *secret* instead.

If a *secret* is provided, a second cookie with the name *name + ".sig"* will
be read and its value will be verified as the cookie value's signature.

If the cookie is not set or its signature is invalid, "undefined" will be returned instead.

@EXAMPLES

```
var sid = request.cookie("sid", {signed: "keyboardcat"});
```

