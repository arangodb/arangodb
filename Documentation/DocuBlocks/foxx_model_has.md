


`FoxxModel#has(name)`

Returns true if the attribute is set to a non-null or non-undefined value.

@EXAMPLES

```js
instance = new Model({
  a: 1
});

instance.has("a"); //=> true
instance.has("b"); //=> false
```

