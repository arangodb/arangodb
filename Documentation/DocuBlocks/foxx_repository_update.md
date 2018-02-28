

`FoxxRepository#update(model, object)`

Find the model in the database by its *_id* and update it with the given object.
Expects a model. Sets the revision of the model and updates its properties.
Returns the model.

@EXAMPLES

```javascript
repository.update(myModel, {name: 'Jan Steeman'});
```

