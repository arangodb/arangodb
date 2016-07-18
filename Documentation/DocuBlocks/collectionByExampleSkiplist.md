

@brief constructs a query-by-example using a skiplist index
`collection.byExampleSkiplist(index, example)`

Selects all documents from the specified skiplist index that match the
specified example and returns a cursor.

You can use *toArray*, *next*, or *hasNext* to access the
result. The result can be limited using the *skip* and *limit*
operator.

An attribute name of the form *a.b* is interpreted as attribute path,
not as attribute. If you use

```json
{ "a": { "c": 1 } }
```

as example, then you will find all documents, such that the attribute
*a* contains a document of the form *{ "c": 1 }*. For example the document

```json
{ "a" : { "c" : 1 }, "b" : 1 }
```

will match, but the document

```json
{ "a" : { "c" : 1, "b" : 1 } }
```

will not.

However, if you use

```json
{ "a.c" : 1 }
```

then you will find all documents, which contain a sub-document in *a*
that has an attribute *c* of value *1*. Both the following documents

```json
{ "a" : { "c" : 1 }, "b" : 1 }

```

and

```json
{ "a" : { "c" : 1, "b" : 1 } }
```

will match.

`collection.byExampleSkiplist(index, path1, value1, ...)`

