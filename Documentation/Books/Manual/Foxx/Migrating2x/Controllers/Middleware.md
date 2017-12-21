Before, after and around
========================

The `before`, `after` and `around` methods can easily be replaced by middleware:

Old:

```js
let start;
ctrl.before(function (req, res) {
  start = Date.now();
});
ctrl.after(function (req, res) {
  console.log('Request handled in ', (Date.now() - start), 'ms');
});
```

New:

```js
router.use(function (req, res, next) {
  let start = Date.now();
  next();
  console.log('Request handled in ', (Date.now() - start), 'ms');
});
```

Note that unlike `around` middleware receives the `next` function as the *third* argument (the "opts" argument has no equivalent).
