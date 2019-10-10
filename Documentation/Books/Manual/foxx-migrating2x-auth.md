---
layout: default
description: Auth and OAuth2
---
Auth and OAuth2
===============

The `util-simple-auth` and `util-oauth2` Foxx services have been replaced with the [Foxx auth](foxx-reference-modules-auth.html)
and [Foxx OAuth2](foxx-reference-modules-oauth2.html) modules.
It is no longer necessary to install these services as dependencies in order to use the functionality.

Old:

```js
'use strict';
const auth = applicationContext.dependencies.simpleAuth;

// ...

const valid = auth.verifyPassword(authData, password);
```

New:

```js
'use strict';
const createAuth = require('@arangodb/foxx/auth');
const auth = createAuth(); // Use default configuration

// ...

const valid = auth.verifyPassword(authData, password);
```
