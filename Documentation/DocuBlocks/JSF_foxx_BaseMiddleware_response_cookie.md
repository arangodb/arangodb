


`response.cookie(name, value, cfg)`

Add a cookie to the response. Optionally the cookie can be signed.

*Parameter*

* *name*: the name of the cookie to add to the response.
* *value*: the value of the cookie to add to the response.
* *cfg* (optional): an object with any of the following properties:
  * *ttl* (optional): the number of seconds until this cookie expires.
  * *path* (optional): the cookie path.
  * *domain* (optional): the cookie domain.
  * *secure* (optional): mark the cookie as safe transport (HTTPS) only.
  * *httpOnly* (optional): mark the cookie as HTTP(S) only.
  * *signed* (optional): an object with any of the following properties:
    * *secret*: a secret string to sign the cookie with.
    * *algorithm*: hashing algorithm to sign the cookie with. Default: *"sha256"*.

If *signed* is a string, it will be used as the *secret* instead.

If a *secret* is provided, a second cookie with the name *name + ".sig"* will
be added to the response, containing the cookie's HMAC signature.

@EXAMPLES

```
response.cookie("sid", "abcdef", {signed: "keyboardcat"});
```

