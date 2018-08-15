OAuth 1.0a
==========

`const createOAuth1Client = require('@arangodb/foxx/oauth1');`

The OAuth1 module provides abstractions over OAuth 1.0a providers like
Twitter, XING and Tumblr.

**Examples**

```js
const router = createRouter();
const oauth1 = createOAuth1Client({
  // We'll use Twitter for this example
  requestTokenEndpoint: 'https://api.twitter.com/oauth/request_token',
  authEndpoint: 'https://api.twitter.com/oauth/authorize',
  accessTokenEndpoint: 'https://api.twitter.com/oauth/access_token',
  activeUserEndpoint: 'https://api.twitter.com/1.1/account/verify_credentials.json',
  clientId: 'keyboardcat',
  clientSecret: 'keyboardcat'
});

module.context.use('/oauth1', router);

// See the user management example for setting up the
// sessions and users objects used in this example
router.use(sessions);

router.post('/auth', function (req, res) {
  const url = req.reverse('oauth1_callback');
  const oauth_callback = req.makeAbsolute(url);
  const requestToken = oauth1.fetchRequestToken(oauth_callback);
  if (requestToken.oauth_callback_confirmed !== 'true') {
    res.throw(500, 'Could not fetch OAuth request token');
  }
  // Set request token cookie for five minutes
  res.cookie('oauth1_request_token', requestToken.oauth_token, {ttl: 60 * 5});
  // Redirect to the provider's authorization URL
  res.redirect(303, oauth1.getAuthUrl(requestToken.oauth_token));
});

router.get('/auth', function (req, res) {
  // Make sure CSRF cookie matches the URL
  const expectedToken = req.cookie('oauth1_request_token');
  if (!expectedToken || req.queryParams.oauth_token !== expectedToken) {
    res.throw(400, 'CSRF mismatch.');
  }
  const authData = oauth1.exchangeRequestToken(
    req.queryParams.oauth_token,
    req.queryParams.oauth_verifier
  );
  const twitterToken = authData.oauth_token;
  const twitterSecret = authData.oauth_token_secret;
  // Fetch the active user's profile info
  const profile = oauth1.fetchActiveUser(twitterToken, twitterSecret);
  const twitterId = profile.screen_name;
  // Try to find an existing user with the user ID
  // (this requires the users collection)
  let user = users.firstExample({twitterId});
  if (user) {
    // Update the twitterToken if it has changed
    if (
      user.twitterToken !== twitterToken ||
      user.twitterSecret !== twitterSecret
    ) {
      users.update(user, {twitterToken, twitterSecret});
    }
  } else {
    // Create a new user document
    user = {
      username: `twitter:${twitterId}`,
      twitterId,
      twitterToken
    }
    const meta = users.save(user);
    Object.assign(user, meta);
  }
  // Log the user in (this requires the session middleware)
  req.session.uid = user._key;
  req.session.twitterToken = authData.twitterToken;
  req.session.twitterSecret = authData.twitterSecret;
  req.sessionStorage.save(req.session);
  // Redirect to the default route
  res.redirect(303, req.makeAbsolute('/'));
}, 'oauth1_callback')
.queryParam('oauth_token', joi.string().optional())
.queryParam('oauth_verifier', joi.string().optional());
```

Creating an OAuth1.0a client
----------------------------

`createOAuth1Client(options): OAuth1Client`

Creates an OAuth1.0a client.

**Arguments**

* **options**: `Object`

  An object with the following properties:

  * **requestTokenEndpoint**: `string`

    The fully-qualified URL of the provider's
    [Temporary Credentials Request endpoint](https://tools.ietf.org/html/rfc5849#section-2.1).
    This URL is used to fetch the unauthenticated temporary credentials that
    will be used to generate the authorization redirect for the user.

  * **authEndpoint**: `string`

    The fully-qualified URL of the provider's
    [Resource Owner Authorization endpoint](https://tools.ietf.org/html/rfc5849#section-2.2).
    This is the URL the user will be redirected to in order to authorize the
    OAuth consumer (i.e. your service).

  * **accessTokenEndpoint**: `string`

    The fully-qualified URL of the provider's
    [Token Request endpoint](https://tools.ietf.org/html/rfc5849#section-2.3).
    This URL is used to exchange the authenticated temporary credentials
    received from the authorization redirect for the actual token credentials
    that can be used to make requests to the API server.

  * **activeUserEndpoint**: `string` (optional)

    The fully-qualified URL of the provider's endpoint for fetching details
    about the current user.

  * **clientId**: `string`

    The application's *Client ID* (or *Consumer Key*) for the provider.

  * **clientSecret**: `string`

    The application's *Client Secret* (or *Consumer Secret*) for the provider.

  * **signatureMethod**: `string` (Default: `"HMAC-SHA1"`)

    The cryptographic method that will be used to sign OAuth 1.0a requests.
    Only `"HMAC-SHA1-"` and `"PLAINTEXT"` are supported at this time.

    Note that many providers may not implement `"PLAINTEXT"` as it exposes the
    *Client Secret* and `oauth_token_secret` instead of generating a signature.

Returns an OAuth 1.0a client for the given provider.

### Setting up OAuth 1.0a for Twitter

If you want to use Twitter as the OAuth 1.0a provider, use the following options:

* *requestTokenEndpoint*: `https://api.twitter.com/oauth/request_token`
* *authEndpoint*: `https://api.twitter.com/oauth/authorize`
* *accessTokenEndpoint*: `https://api.twitter.com/oauth/access_token`
* *activeUserEndpoint*: `https://api.twitter.com/1.1/account/verify_credentials.json`

You also need to obtain a client ID and client secret from Twitter:

1. Create a regular account at [Twitter](https://www.twitter.com) or use an
   existing account you own.
2. Visit the [Twitter Application Management](https://apps.twitter.com)
   dashboard and sign in with your Twitter account.
3. Click on *Create New App* and follow the instructions provided.
   The *Callback URL* should match your *oauth_callback* later. You may be
   prompted to add a mobile phone number to your account and verify it.
4. Open the *Keys and Access Tones* tab, then note down the *Consumer Key*
   and *Consumer Secret*.
5. Set the option *clientId* to the *Consumer Key* and the option
   *clientSecret* to the *Consumer Secret*.

Note that if you only need read-only access to public information, you can also
[use the *clientId* and *clientSecret* directly](https://dev.twitter.com/oauth/application-only)
without OAuth 1.0a.

See [Twitter REST API Reference Documentation](https://dev.twitter.com/rest/reference).

### Setting up OAuth 1.0a for XING

If you want to use XING as the OAuth 1.0a provider, use the following options:

* *requestTokenEndpoint*: `https://api.xing.com/v1/request_token`
* *authEndpoint*: `https://api.xing.com/v1/authorize`
* *accessTokenEndpoint*: `https://api.xing.com/v1/access_token`
* *activeUserEndpoint*: `https://api.xing.com/v1/users/me`

You also need to obtain a client ID and client secret from XING:

1. Create a regular account at [XING](https://xing.com) or use an existing
   account you own.
2. Visit the [XING Developer](https://dev.xing.com) page and sign in with
   your XING account.
3. Click on *Create app* and note down the *Consumer key* and *Consumer secret*.
4. Set the option *clientId* to the *Consumer key* and the option
   *clientSecret* to the *Consumer secret*.

See [XING Developer Documentation](https://dev.xing.com/docs).

### Setting up OAuth 1.0a for Tumblr

If you want to use Tumblr as the OAuth 1.0a provider, use the following options:

* *requestTokenEndpoint*: `https://www.tumblr.com/oauth/request_token`
* *authEndpoint*: `https://www.tumblr.com/oauth/authorize`
* *accessTokenEndpoint*: `https://www.tumblr.com/oauth/access_token`
* *activeUserEndpoint*: `https://api.tumblr.com/v2/user/info`

You also need to obtain a client ID and client secret from Tumblr:

1. Create a regular account at [Tumblr](https://www.tumblr.com) or use an
   existing account you own.
2. Visit the [Tumblr Applications](https://www.tumblr.com/oauth/apps) dashboard.
3. Click on *Register application*, then follow the instructions provided.
   The *Default callback URL* should match your *oauth_callback* later.
4. Note down the *OAuth Consumer Key* and *Secret Key*. The secret may be
   hidden by default.
5. Set the option *clientId* to the *OAuth Consumer Key* and the option
   *clientSecret* to the *Secret Key*.

See [Tumblr API Documentation](https://www.tumblr.com/docs/en/api/v2).

Fetch an unauthenticated request token
--------------------------------------

`oauth1.fetchRequestToken(oauth_callback, opts)`

Fetches an `oauth_token` that can be used to create an authorization URL that
redirects to the given `oauth_callback` on confirmation.

Performs a *POST* response to the *requestTokenEndpoint*.

Throws an exception if the remote server responds with an empty response body.

**Arguments**

* **oauth_callback**: `string`

  The fully-qualified URL of your application's OAuth 1.0a callback.

* **opts**: `Object` (optional)

  An object with additional query parameters to include in the request.

  See [RFC 5849](https://tools.ietf.org/html/rfc5849).

Returns the parsed response object.

Get the authorization URL
-------------------------

`oauth1.getAuthUrl(oauth_token, opts): string`

Generates the authorization URL for the authorization endpoint.

**Arguments**

* **oauth_token**: `string`

  The `oauth_token` previously returned by `fetchRequestToken`.

* **opts**: (optional)

  An object with additional query parameters to add to the URL.

  See [RFC 5849](https://tools.ietf.org/html/rfc5849).

Returns a fully-qualified URL for the authorization endpoint of the provider
by appending the `oauth_token` and any additional arguments from *opts* to
the *authEndpoint*.

**Examples**

```js
const requestToken = oauth1.fetchRequestToken(oauth_callback);
if (requestToken.oauth_callback_confirmed !== 'true') {
  throw new Error('Provider could not confirm OAuth 1.0 callback');
}
const authUrl = oauth1.getAuthUrl(requestToken.oauth_token);
```

Exchange an authenticated request token for an access token
-----------------------------------------------------------

`oauth1.exchangeRequestToken(oauth_token, oauth_verifier, opts)`

Takes a pair of authenticated temporary credentials passed to the callback URL
by the provider and exchanges it for an `oauth_token` and `oauth_token_secret`
than can be used to perform authenticated requests to the OAuth 1.0a provider.

Performs a *POST* response to the *accessTokenEndpoint*.

Throws an exception if the remote server responds with an empty response body.

**Arguments**

* **oauth_token**: `string`

  The `oauth_token` passed to the callback URL by the provider.

* **oauth_verifier**: `string`

  The `oauth_verifier` passed to the callback URL by the provider.

* **opts**: `Object` (optional)

  An object with additional query parameters to include in the request.

  See [RFC 5849](https://tools.ietf.org/html/rfc5849).

Returns the parsed response object.

Fetch the active user
---------------------

`oauth1.fetchActiveUser(oauth_token, oauth_token_secret, opts): Object`

Fetches details of the active user.

Performs a *GET* response to the *activeUserEndpoint*.

Throws an exception if the remote server responds with an empty response body.

Returns `null` if the *activeUserEndpoint* is not configured.

**Arguments**

* **oauth_token**: `string`

  An OAuth 1.0a access token as returned by *exchangeRequestToken*.

* **oauth_token_secret**: `string`

  An OAuth 1.0a access token secret as returned by *exchangeRequestToken*.

* **opts**: `Object` (optional)

  An object with additional query parameters to include in the request.

  See [RFC 5849](https://tools.ietf.org/html/rfc5849).

Returns the parsed response object.

**Examples**

```js
const authData = oauth1.exchangeRequestToken(oauth_token, oauth_verifier);
const userData = oauth1.fetchActiveUser(authData.oauth_token, authData.oauth_token_secret);
```

Create an authenticated request object
--------------------------------------

`oauth1.createSignedRequest(method, url, parameters, oauth_token, oauth_token_secret)`

Creates a request object that can be used to perform a request to the OAuth 1.0a
provider with the provided token credentials.

**Arguments**

* **method**: `string`

  HTTP method the request will use, e.g. `"POST"`.

* **url**: `string`

  The fully-qualified URL of the provider the request will be performed against.

  The URL may optionally contain any number of query parameters.

* **parameters**: `string | Object | null`

  An additional object or query string containing query parameters or body
  parameters that will be part of the signed request.

* **oauth_token**: `string`

  An OAuth 1.0a access token as returned by *exchangeRequestToken*.

* **oauth_token_secret**: `string`

  An OAuth 1.0a access token secret as returned by *exchangeRequestToken*.

Returns an object with three properties:

 * **url**: The normalized URL without any query parameters.

 * **qs**: A normalized query string containing all *parameters* and query parameters.

 * **headers**: An object containing the following properties:

   * **accept**: The string `"application/json"`.

   * **authorization**: An OAuth authorization header containing all OAuth
     parameters and the request signature.

**Examples**

Fetch a list of tweets mentioning `@arangodb`:

```js
const request = require('@arangodb/request');
const req = oauth1.createSignedRequest(
  'GET',
  'https://api.twitter.com/1.1/search/tweets.json',
  {q: '@arangodb'},
  authData.oauth_token,
  authData.oauth_token_secret
);
const res = request(req);
console.log(res.json.statuses);
```

Signing a more complex request:

```js
const url = 'https://api.example.com/v1/timeline?visible=public';
const params = {hello: 'world', longcat: 'is long'};
const req = oauth1.createSignedRequest(
  'POST',
  url, // URL includes a query parameter that will be signed
  params, // Request body needs to be signed too
  authData.oauth_token,
  authData.oauth_token_secret
);
const res = request.post(url, {
  form: params,
  headers: {
    accept: 'application/x-www-form-urlencoded',
    // Authorization header includes the signature
    authorization: req.headers.authorization
  }
});
console.log(res.json);
```
