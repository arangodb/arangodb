

`FoxxRepository#byId(id)`

Returns the model for the given ID ("collection/key") or "key".

@EXAMPLES

```javascript
var byIdModel = repository.byId('test/12411');
byIdModel.get('name');

var byKeyModel = repository.byId('12412');
byKeyModel.get('name');
```

