

`FoxxRepository#all()`

Returns an array of models that matches the given example. You can provide
both a skip and a limit value.

**Warning:** ArangoDB doesn't guarantee a specific order in this case, to make
this really useful we have to explicitly provide something to order by.

*Parameter*

* *options* (optional):
  * *skip* (optional): skips the first given number of models.
  * *limit* (optional): only returns at most the given number of models.

@EXAMPLES

```javascript
var myModel = repository.all({ skip: 4, limit: 2 });
myModel[0].get('name');
```

