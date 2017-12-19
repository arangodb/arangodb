Sessions
========

The `ctrl.activateSessions` method and the related `util-sessions-local` Foxx service have been replaced with the [Foxx sessions](../Sessions/README.md) middleware. It is no longer possible to use the built-in session storage but you can simply pass in any document collection directly.

Old:

```js
const localSessions = applicationContext.dependencies.localSessions;
const sessionStorage = localSessions.sessionStorage;
ctrl.activateSessions({
  sessionStorage: sessionStorage,
  cookie: {secret: 'keyboardcat'}
});

ctrl.destroySession('/logout', function (req, res) {
  res.json({message: 'Goodbye!'});
});
```

New:

```js
const sessionMiddleware = require('@arangodb/foxx/sessions');
const cookieTransport = require('@arangodb/foxx/sessions/transports/cookie');
router.use(sessionMiddleware({
  storage: module.context.collection('sessions'),
  transport: cookieTransport('keyboardcat')
}));

router.post('/logout', function (req, res) {
  req.sessionStorage.clear(req.session);
  res.json({message: 'Goodbye!'});
});
```
