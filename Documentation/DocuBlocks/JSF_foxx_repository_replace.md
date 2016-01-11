

`FoxxRepository#replace(model)`

Find the model in the database by its *_id* and replace it with this version.
Expects a model. Sets the revision of the model.
Returns the model.

@EXAMPLES

```javascript
myModel.set('name', 'Jan Steemann');
repository.replace(myModel);
```

