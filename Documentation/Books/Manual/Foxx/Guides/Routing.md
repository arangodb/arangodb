# Nested routers

Don't have to define service all in one file. Don't have to mount routers where you define them.

Have a folder structure like e.g.
```
api/
  index.js
  auth.js
  tickets.js
  admin/
    index.js
    users.js
```

Can define router per file and export/import, mount with or without prefix in parent router.

## Guarded child routers

Use middleware per router. Example: guard admin router (and child routers) with auth middleware like
```js
router.use((req, res, next) => {
  try {
    req.user = users.document(req.session.uid);
  } catch (e) {
    res.throw('forbidden');
  }
});
```
guards entire subtree but leaves parent router alone.
