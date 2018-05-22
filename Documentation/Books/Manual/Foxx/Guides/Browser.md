Exposing Foxx to the browser
============================

There are three ways to use Foxx in a web application:

1. Accessing Foxx from an application server that exposes its own API.

2. Using a web server like Apache or nginx as a reverse proxy to expose only the Foxx service.

3. Exposing ArangoDB directly by running ArangoDB on a public port.

Using an application server
---------------------------

Accessing Foxx from an application server is probably the safest approach as the application server shields the database from the browser entirely. However this also adds the most development overhead and may result in unnecessary duplication of access logic.

This approach works best if you're using Foxx in an existing application stack or want to use an [ArangoDB driver](https://www.arangodb.com/arangodb-drivers/) to access the database API directly alongside your Foxx service.

As Foxx services provide ordinary HTTP endpoints, you can access them from your existing application server using any run-of-the-mill HTTP client with JSON support. Some ArangoDB drivers also let you access arbitrary HTTP endpoints:

```js
const express = require('express');
const app = express();
const { Database } = require('arangojs');
const db = new Database();
const service = db.route('/my-foxx');
app.get('/', async function (req, res) {
  const response = await service.get('/hello');
  res.status(response.statusCode);
  res.write(response.body);
  res.end();
});
app.listen(9000);
```

Using a reverse proxy
---------------------

For information on setting up the Apache web server as a reverse proxy check [the official Apache 2.4 documentation](https://httpd.apache.org/docs/2.4/howto/reverse_proxy.html). For nginx check [the nginx admin guide](https://docs.nginx.com/nginx/admin-guide/web-server/reverse-proxy/). Similar documentation exists for [lighttpd](https://redmine.lighttpd.net/projects/1/wiki/Docs_ModProxy) and [Microsoft IIS](https://blogs.msdn.microsoft.com/friis/2016/08/25/setup-iis-with-url-rewrite-as-a-reverse-proxy-for-real-world-apps/).

Example (nginx):

```nginx
location /api/ {
    proxy_pass http://localhost:8529/_db/_system/my-foxx/;
}
```

The advantage of this approach is that it allows you to expose just the service itself without exposing the entire database API.

This approach also works well if you're already using a web server to serve your web application frontend files and want your frontend to talk directly to the service.

Note that when running Foxx behind a reverse proxy some properties of the request object will reflect the proxy rather than the original request source (i.e. the browser). You can tell your service to expect to run behind a trusted proxy by enabling the `trustProxy` property of the service context:

```js
// in your main entry file, e.g. index.js
module.context.trustProxy = true;
```

Foxx will then trust the values of the following request headers:

* `x-forwarded-proto` for `req.protocol`
* `x-forwarded-host` for `req.hostname` and `req.port`
* `x-forwarded-port` for `req.port`
* `x-forwarded-for` for `req.remoteAddress` and `req.remoteAddresses`

Note that this property needs to be set in your main entry file. Setting it in the setup script has no effect.

Exposing ArangoDB directly
--------------------------

This is the most obvious but also most dangerous way to expose your Foxx service. Running ArangoDB on a public port will expose the entire database API and allow anyone who can guess your database credentials to do whatever they want.

Unless your service is explicitly intended to be used by people who already have access to the ArangoDB web interface, you should go with one of the other approaches instead.

**Note:** Seriously, only use this for internal services intended to help users who already have full access to the database. Don't ever expose your database to the public Internet.

### Cross-Origin Resource Sharing (CORS)

If you are running ArangoDB on a public port and want a web app running on a different port or domain to access it, you will need to enable CORS in ArangoDB.

First you need to [configure ArangoDB for CORS](../../../HTTP/General/index.html#cross-origin-resource-sharing-cors-requests). As of 3.2 Foxx will then automatically whitelist all response headers as they are used.

If you want more control over the whitelist or are using an older version of ArangoDB you can set the following response headers in your request handler:

* `access-control-expose-headers`: a comma-separated list of response headers. This defaults to a list of all headers the response is actually using (but not including any `access-control` headers).

* `access-control-allow-credentials`: can be set to `"false"` to forbid exposing cookies. The default value depends on whether ArangoDB trusts the origin. See the [notes on `http.trusted-origin`](../../../HTTP/General/index.html#cookies-and-authentication).

Note that it is not possible to override these headers for the CORS preflight response. It is therefore not possible to accept credentials or cookies only for individual routes, services or databases. The origin needs to be trusted according to the general ArangoDB configuration (see above).
