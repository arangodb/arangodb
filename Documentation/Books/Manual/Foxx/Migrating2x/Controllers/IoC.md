Dependency injection
====================

There is no equivalent of the `addInjector` method available in ArangoDB 2.x controllers. Most use cases can be solved by simply using plain variables but if you need something more flexible you can also use middleware:

Old:

```js
ctrl.addInjector('magicNumber', function () {
  return Math.random();
});

ctrl.get('/', function (req, res, injected) {
  res.json(injected.magicNumber);
});
```

New:

```js
function magicMiddleware(name) {
  return {
    register () {
      let magic;
      return function (req, res, next) {
        if (!magic) {
          magic = Math.random();
        }
        req[name] = magic;
        next();
      };
    }
  };
}

router.use(magicMiddleware('magicNumber'));

router.get('/', function (req, res) {
  res.json(req.magicNumber);
});
```

Or simply:

```js
const magicNumber = Math.random();

router.get('/', function (req, res) {
  res.json(magicNumber);
});
```
