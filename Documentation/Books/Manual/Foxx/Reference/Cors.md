Cross-Origin Resource Sharing (CORS)
====================================

To use CORS in your Foxx services you first need to [configure ArangoDB for CORS](../../../HTTP/General/index.html#cross-origin-resource-sharing-cors-requests). As of 3.2 Foxx will then automatically whitelist all response headers as they are used.

If you want more control over the whitelist or are using an older version of ArangoDB you can set the following response headers in your request handler:

* `access-control-expose-headers`: a comma-separated list of response headers. This defaults to a list of all headers the response is actually using (but not including any `access-control` headers).

* `access-control-allow-credentials`: can be set to `"false"` to forbid exposing cookies. The default value depends on whether ArangoDB trusts the origin. See the [notes on `http.trusted-origin`](../../../HTTP/General/index.html#cookies-and-authentication).

Note that it is not possible to override these headers for the CORS preflight response. It is therefore not possible to accept credentials or cookies only for individual routes, services or databases. The origin needs to be trusted according to the general ArangoDB configuration (see above).
