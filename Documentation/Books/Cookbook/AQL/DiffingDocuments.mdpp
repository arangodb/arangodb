# Diffing Two Documents in AQL

## Problem

How to create a `diff` of documents in AQL

## Solution

Though there is no built-in AQL function to `diff` two documents, it is easily possible to build your own like in the following query:

```
/* input document 1*/
LET doc1 = {
  "foo" : "bar",
  "a" : 1,
  "b" : 2
}

/* input document 2 */
LET doc2 = {
  "foo" : "baz",
  "a" : 2,
  "c" : 3
}

/* collect attributes present in doc1, but missing in doc2 */
LET missing = (
  FOR key IN ATTRIBUTES(doc1)
  FILTER ! HAS(doc2, key)
  RETURN {
    [ key ]: doc1[key]
  }
)

/* collect attributes present in both docs, but that have different values */
LET changed = (
  FOR key IN ATTRIBUTES(doc1)
    FILTER HAS(doc2, key) && doc1[key] != doc2[key]
    RETURN {
      [ key ] : {
        old: doc1[key],
        new: doc2[key]
      }
    }
)

/* collect attributes present in doc2, but missing in doc1 */
LET added = (
  FOR key IN ATTRIBUTES(doc2)
    FILTER ! HAS(doc1, key)
    RETURN {
      [ key ] : doc2[key]
    }
)

/* return final result */
RETURN {
  "missing" : missing,
  "changed" : changed,
  "added" : added
}
```

**Note**: The query may look a bit lengthy, but much of that is due to formatting. A more terse version can be found below.

The above query will return a document with three attributes:

- _missing_: Contains all attributes only present in first document (i.e. missing in second document)
- _changed_: Contains all attributes present in both documents that have different values
- _added_: Contains all attributes only present in second document (i.e. missing in first document)

For the two example documents it will return:

```json
[
 {
   "missing" : [
     {
       "b" : 2
     }
   ],
   "changed" : [
     {
       "foo" : {
         "old" : "bar",
         "new" : "baz"
       }
      },
     {
       "a" : {
         "old" : 1,
         "new" : 2
       }
     }
   ],
   "added" : [
     {
       "c" : 3
     }
   ]
 }
]
```


That output format was the first that came to my mind. It is of course possible to adjust the query so it produces a different output format.

Following is a version of the same query that can be invoked from JavaScript easily. It passes the two documents as bind parameters and calls `db._query`. The query is now an one-liner (less readable but easier to copy&paste):

```js
bindVariables = {
  doc1 : { "foo" : "bar", "a" : 1, "b" : 2 },
  doc2 : { "foo" : "baz", "a" : 2, "c" : 3 }
};

query = "LET doc1 = @doc1, doc2 = @doc2, missing = (FOR key IN ATTRIBUTES(doc1) FILTER ! HAS(doc2, key) RETURN { [ key ]: doc1[key] }), changed = (FOR key IN ATTRIBUTES(doc1) FILTER HAS(doc2, key) && doc1[key] != doc2[key] RETURN { [ key ] : { old: doc1[key], new: doc2[key] } }), added = (FOR key IN ATTRIBUTES(doc2) FILTER ! HAS(doc1, key) RETURN { [ key ] : doc2[key] }) RETURN { missing : missing, changed : changed, added : added }";

result = db._query(query, bindVariables).toArray();
```

**Author:** [Jan Steemann](https://github.com/jsteemann)

**Tags**: #howto #aql
