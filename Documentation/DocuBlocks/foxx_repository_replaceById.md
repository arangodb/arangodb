

`FoxxRepository#replaceById(id, object)`

Find the item in the database by the given ID  ("collection/key") or "key"
and replace it with the given object's attributes.

If the object is a model, updates the model's revision and returns the model.

@EXAMPLES

```javascript
repository.replaceById('test/123345', myNewModel);
repository.replaceById('123346', myNewModel);
```

