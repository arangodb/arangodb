Foxx queries
============

The `createQuery` method has been removed. It can be trivially replaced with plain JavaScript functions and direct calls to [the `db._query` method](../Modules.md):

Old:

```js
'use strict';
const Foxx = require('org/arangodb/foxx');
const query = Foxx.createQuery({
    query: 'FOR u IN _users SORT u.user ASC RETURN u[@propName]',
    params: ['propName'],
    transform: function (results, uppercase) {
        return (
          uppercase
          ? results[0].toUpperCase()
          : results[0].toLowerCase()
        );
    }
});

query('user', true);
```

New:

```js
'use strict';
const db = require('@arangodb').db;
const aql = require('@arangodb').aql;

function query(propName, uppercase) {
  const results = db._query(aql`
    FOR u IN _users
    SORT u.user ASC
    RETURN u[${propName}]
  `);
  return (
    uppercase
    ? results[0].toUpperCase()
    : results[0].toLowerCase()
  );
}

query('user', true);
```
