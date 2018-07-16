

`FoxxRepository#fulltext(attribute, query, options)`

Returns all models whose attribute *attribute* matches the search query
*query*.

In order to use the fulltext method, a fulltext index must be defined on
the repository. If multiple fulltext indexes are defined on the repository
for the attribute, the most capable one will be selected.
If no fulltext index is present, the method will not be available.

*Parameter*

* *attribute*: model attribute to perform a search on.
* *query*: query to match the attribute against.
* *options* (optional):
  * *limit* (optional): number of models to return. Defaults to all.

@EXAMPLES

```javascript
repository.fulltext("text", "word", {limit: 1});
```

