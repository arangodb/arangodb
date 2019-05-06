OAuth 2.0
=========

`const createOAuth2Client = require('@arangodb/foxx/oauth2');`

The OAuth2 module provides abstractions over OAuth 2.0 providers like
Facebook, GitHub and Google.

**Examples**

```js
const crypto = require('@arangodb/crypto');
const router = createRouter();
const oauth2 = createOAuth2Client({
  // We'll use Facebook for this example
  authEndpoint: 'https://www.facebook.com/dialog/oauth',
  tokenEndpoint: 'https://graph.facebook.com/oauth/access_token',
  activeUserEndpoint: 'https://graph.facebook.com/v2.0/me',
  clientId: 'keyboardcat',
  clientSecret: 'keyboardcat'
});

module.context.use('/oauth2', router);

// See the user management example for setting up the
// sessions and users objects used in this example
router.use(sessions);

router.post('/auth', function (req, res) {
  const csrfToken = crypto.genRandomAlphaNumbers(32);
  const url = req.reverse('oauth2_callback', {csrfToken});
  const redirect_uri = req.makeAbsolute(url);
  // Set CSRF cookie for five minutes
  res.cookie('oauth2_csrf_token', csrfToken, {ttl: 60 * 5});
  // Redirect to the provider's authorization URL
  res.redirect(303, oauth2.getAuthUrl(redirect_uri));
});

router.get('/auth', function (req, res) {
  // Some providers pass errors as query parameter
  if (req.queryParams.error) {
    res.throw(500, `Provider error: ${req.queryParams.error}`)
  }
  // Make sure CSRF cookie matches the URL
  const expectedToken = req.cookie('oauth2_csrf_token');
  if (!expectedToken || req.queryParams.csrfToken !== expectedToken) {
    res.throw(400, 'CSRF mismatch.');
  }
  // Make sure the URL contains a grant token
  if (!req.queryParams.code) {
    res.throw(400, 'Provider did not pass grant token.');
  }
  // Reconstruct the redirect_uri used for the grant token
  const url = req.reverse('oauth2_callback');
  const redirect_uri = req.makeAbsolute(url);
  // Fetch an access token from the provider
  const authData = oauth2.exchangeGrantToken(
    req.queryParams.code,
    redirect_uri
  );
  const facebookToken = authData.access_token;
  // Fetch the active user's profile info
  const profile = oauth2.fetchActiveUser(facebookToken);
  const facebookId = profile.id;
  // Try to find an existing user with the user ID
  // (this requires the users collection)
  let user = users.firstExample({facebookId});
  if (user) {
    // Update the facebookToken if it has changed
    if (user.facebookToken !== facebookToken) {
      users.update(user, {facebookToken});
    }
  } else {
    // Create a new user document
    user = {
      username: `fb:${facebookId}`,
      facebookId,
      facebookToken
    }
    const meta = users.save(user);
    Object.assign(user, meta);
  }
  // Log the user in (this requires the session middleware)
  req.session.uid = user._key;
  req.session.facebookToken = authData.facebookToken;
  req.sessionStorage.save(req.session);
  // Redirect to the default route
  res.redirect(303, req.makeAbsolute('/'));
}, 'oauth2_callback')
.queryParam('error', joi.string().optional())
.queryParam('csrfToken', joi.string().optional())
.queryParam('code', joi.string().optional());
```

Creating an OAuth 2.0 client
-------------------------

`createOAuth2Client(options): OAuth2Client`

Creates an OAuth 2.0 client.

**Arguments**

* **options**: `object`

  An object with the following properties:

  * **authEndpoint**: `string`

    The fully-qualified URL of the provider's
    [authorization endpoint](http://tools.ietf.org/html/rfc6749#section-3.1).

  * **tokenEndpoint**: `string`

    The fully-qualified URL of the provider's
    [token endpoint](http://tools.ietf.org/html/rfc6749#section-3.2).

  * **refreshEndpoint**: `string` (optional)

    The fully-qualified URL of the provider's
    [refresh token endpoint](http://tools.ietf.org/html/rfc6749#section-6).

  * **activeUserEndpoint**: `string` (optional)

    The fully-qualified URL of the provider's endpoint for fetching
    details about the current user.

  * **clientId**: `string`

    The application's *Client ID* (or *App ID*) for the provider.

  * **clientSecret**: `string`

    The application's *Client Secret* (or *App Secret*) for the provider.

Returns an OAuth 2.0 client for the given provider.

### Setting up OAuth 2.0 for Facebook

If you want to use Facebook as the OAuth 2.0 provider, use the following options:

* *authEndpoint*: `https://www.facebook.com/dialog/oauth`
* *tokenEndpoint*: `https://graph.facebook.com/oauth/access_token`
* *activeUserEndpoint*: `https://graph.facebook.com/v2.0/me`

You also need to obtain a client ID and client secret from Facebook:

1. Create a regular account at [Facebook](https://www.facebook.com) or use an
   existing account you own.
2. Visit the [Facebook Developers](https://developers.facebook.com) page.
3. Click on *Apps* in the menu, then select *Register as a Developer*
   (the only option) and follow the instructions provided. You may need to
   verify your account by phone.
4. Click on *Apps* in the menu, then select *Create a New App* and follow
   the instructions provided.
5. Open the app dashboard, then note down the *App ID* and *App Secret*.
   The secret may be hidden by default.
6. Click on *Settings*, then *Advanced* and enter one or more
   *Valid OAuth redirect URIs*. At least one of them must match your
   *redirect_uri* later. Don't forget to save your changes.
7. Set the option *clientId* to the *App ID* and the option *clientSecret*
   to the *App Secret*.

### Setting up OAuth 2.0 for GitHub

If you want to use GitHub as the OAuth 2.0 provider, use the following options:

* *authEndpoint*: `https://github.com/login/oauth/authorize?scope=user`
* *tokenEndpoint*: `https://github.com/login/oauth/access_token`
* *activeUserEndpoint*: `https://api.github.com/user`

You also need to obtain a client ID and client secret from GitHub:

1. Create a regular account at [GitHub](https://github.com) or use an
   existing account you own.
2. Go to [Account Settings > Applications > Register new application](https://github.com/settings/applications/new).
3. Provide an *authorization callback URL*. This must match your
   *redirect_uri* later.
4. Fill in the other required details and follow the instructions provided.
5. Open the application page, then note down the *Client ID* and *Client Secret*.
6. Set the option *clientId* to the *Client ID* and the option *clientSecret*
   to the *Client Secret*.

### Setting up OAuth 2.0 for Google

If you want to use Google as the OAuth 2.0 provider, use the following options:

* *authEndpoint*: `https://accounts.google.com/o/oauth2/auth?access_type=offline&scope=profile`
* *tokenEndpoint*: `https://accounts.google.com/o/oauth2/token`
* *activeUserEndpoint*: `https://www.googleapis.com/plus/v1/people/me`

You also need to obtain a client ID and client secret from Google:

1. Create a regular account at [Google](https://www.google.com) or use an
   existing account you own.
2. Visit the [Google Developers Console](https://console.developers.google.com).
3. Click on *Create Project*, then follow the instructions provided.
4. When your project is ready, open the project dashboard, then click on
   *Enable an API*.
5. Enable the *Google+ API* to allow your app to distinguish between different users.
6. Open the *Credentials* page and click *Create new Client ID*, then follow
   the instructions provided. At least one *Authorized Redirect URI* must match
   your *redirect_uri* later. At least one *Authorized JavaScript Origin* must
   match your app's fully-qualified domain.
7. When the Client ID is ready, note down the *Client ID* and *Client secret*.
8. Set the option *clientId* to the *Client ID* and the option *clientSecret*
   to the *Client secret*.

Get the authorization URL
-------------------------

`oauth2.getAuthUrl(redirect_uri, args): string`

Generates the authorization URL for the authorization endpoint.

**Arguments**

* **redirect_uri**: `string`

  The fully-qualified URL of your application's OAuth 2.0 callback.

* **args**: (optional)

  An object with any of the following properties:

  * **response_type**: `string` (Default: `"code"`)

  See [RFC 6749](http://tools.ietf.org/html/rfc6749).

Returns a fully-qualified URL for the authorization endpoint of the provider
by appending the client ID and any additional arguments from *args* to the
*authEndpoint*.

Exchange a grant code for an access token
-----------------------------------------

`oauth2.exchangeGrantToken(code, redirect_uri)`

Exchanges a grant code for an access token.

Performs a *POST* response to the *tokenEndpoint*.

Throws an exception if the remote server responds with an empty response body.

**Arguments**

* **code**: `string`

  A grant code returned by the provider's authorization endpoint.

* **redirect_uri**: `string`

  The original callback URL with which the code was requested.

* **args**: `Object` (optional)

  An object with any of the following properties:

  * **grant_type**: `string` (Default: `"authorization_code"`)

    See [RFC 6749](http://tools.ietf.org/html/rfc6749).

Returns the parsed response object.

Fetch the active user
---------------------

`oauth2.fetchActiveUser(access_token): Object`

Fetches details of the active user.

Performs a *GET* response to the *activeUserEndpoint*.

Throws an exception if the remote server responds with an empty response body.

Returns `null` if the *activeUserEndpoint* is not configured.

**Arguments**

* **access_token**: `string`

  An OAuth 2.0 access token as returned by *exchangeGrantToken*.

Returns the parsed response object.

**Examples**

```js
const authData = oauth2.exchangeGrantToken(code, redirect_uri);
const userData = oauth2.fetchActiveUser(authData.access_token);
```
