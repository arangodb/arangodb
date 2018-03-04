


`new Controller(applicationContext, options)`

This creates a new Controller. The first argument is the controller
context available in the variable *applicationContext*. The second one is an
options array with the following attributes:

* *urlPrefix*: All routes you define within will be prefixed with it.

@EXAMPLES

```js
app = new Controller(applicationContext, {
  urlPrefix: "/meadow"
});
```


