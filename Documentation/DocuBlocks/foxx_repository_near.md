

`FoxxRepository#near(latitude, longitude, options)`

Finds models near the coordinate *(latitude, longitude)*. The returned
list is sorted by distance with the nearest model coming first.

For geo queries it is required that a geo index is present in the
repository. If no geo index is present, the methods will not be available.

*Parameter*

* *latitude*: latitude of the coordinate.
* *longitude*: longitude of the coordinate.
* *options* (optional):
  * *geo* (optional): name of the specific geo index to use.
  * *distance* (optional): If set to a truthy value, the returned models
    will have an additional property containing the distance between the
    given coordinate and the model. If the value is a string, that value
    will be used as the property name, otherwise the name defaults to *"distance"*.
  * *limit* (optional): number of models to return. Defaults to *100*.

@EXAMPLES

```javascript
repository.near(0, 0, {geo: "home", distance: true, limit: 10});
```

