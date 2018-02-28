

`FoxxRepository#updateById(id, object)`

Find an item by ID ("collection/key") or "key" and update it with the
attributes in the provided object.

If the object is a model, updates the model's revision and returns the model.

@EXAMPLES

```javascript
repository.updateById('test/12131', { newAttribute: 'awesome' });
repository.updateById('12132', { newAttribute: 'awesomer' });
```

