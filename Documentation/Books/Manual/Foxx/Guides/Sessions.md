# Sessions

Built-in solution: [session middleware]()

When using collection storage: clean up expired sessions via script:

* extract collection storage into file
* import file where you use storage & in cleanup script
* call script via cron or manually

Cleanup script:
```js
'use strict';
const storage = require('../util/sessionStorage');
module.exports = storage.prune();
```

## Roll your own

Sessions can be implemented easily yourself if you need more control:
```js
const sessions = module.context.collection('sessions');
const secret = module.context.configuration.cookieSecret;
module.context.use((req, res, next) => {
  let sid = req.cookie('sid', {secret});
  if (sid) {
    try {
      req.session = sessions.document(sid);
    } catch (e) {
      sid = null;
      res.cookie('sid', '', {ttl: -1, secret});
    }
  }
  try {
    next();
  } finally {
    if (req.session) {
      if (sid) {
        sessions.update(sid, req.session);
      } else {
        sid = sessions.save(req.session)._key;
      }
      res.cookie('sid', sid, {ttl: 24 * 60 * 60, secret});
    }
  }
})
```
