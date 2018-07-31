Batch Document API
==================

**This proposal was moved to the document repository. Don't change it here
except to convert it to a real documentation.**

**This is a draft**, or at least a basis for discussion.

Note that justifications for several design choices are at the end of the
document, and after that some possible changes to the proposal I'm thinking
about and like to discuss.

## Overview

This API supports the following routes:

```
POST /_api/batch/document/{collection}/read
POST /_api/batch/document/{collection}/insert
POST /_api/batch/document/{collection}/update
POST /_api/batch/document/{collection}/replace
POST /_api/batch/document/{collection}/remove
POST /_api/batch/document/{collection}/upsert
POST /_api/batch/document/{collection}/repsert
```

All details of a request are handled in the request body.

The following request body structure is common for all routes:

```
{
  data: [ {...}, {...}, ... ],
  options: {
    ...
  }
}
```

Requests are always about a list of documents, so `data` must always be an array
(even if, for example, only one document is to be inserted). `data` may be
empty.

All request options (like `waitForSync`) are specified in the `options` object.

Passing unknown attributes will always throw an error. Deprecated attributes
will generate a warning in the server's log file.

The following response body structure is common for all routes:
```
{
  error: <boolean>,
  errorNum: <int>, (optional)
  errorMessage: <string>, (optional)
  errorDataIndex: <int>, (optional)
  results: <array>, (optional)
  ...
}
```

### The structure of an object in the `data` array is as follows.

For `/read`:

```
{
  pattern: {
    _key: <string>,
    ...
  }
}
```

For `/insert`:

```
{
  insertDocument: <object>
}
```

For `/update`:

```
{
  pattern: {
    _key: <string>,
    ...
  },
  updateDocument: <object>
}
```

For `/replace`:

```
{
  pattern: {
    _key: <string>,
    ...
  },
  replaceDocument: <object>
}
```

For `/remove`:

```
{
  pattern: {
    _key: <string>,
    ...
  }
}
```

For `/upsert`:

Note that for upsert, the pattern does not have to contain `_key`.
```
{
  pattern: <object>,
  insertDocument: <object>,
  updateDocument: <object>
}
```

For `/repsert`:

Note that for repsert, the pattern does not have to contain `_key`.
```
{
  pattern: <object>,
  replaceDocument: <object>,
  updateDocument: <object>
}
```

### Options are as follows:

For `/read`:
```
{
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false),
  graphName: <string> (optional),
}
```

For `/insert`:
```
{
  waitForSync: <boolean> (optional, default: false),
  returnNew: <boolean> (optional, default: false),
  silent: <boolean> (optional, default: false),
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false)
  checkGraphs: <boolean> (optional, default: false),
  graphName: <string> (optional),
}
```

For `/update`:
```
{
  waitForSync: <boolean> (optional, default: false),
  returnOld: <boolean> (optional, default: false),
  returnNew: <boolean> (optional, default: false),
  silent: <boolean> (optional, default: false),
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false)
  checkGraphs: <boolean> (optional, default: false),
  graphName: <string> (optional),
}
```

For `/replace`:
```
{
  waitForSync: <boolean> (optional, default: false),
  returnOld: <boolean> (optional, default: false),
  returnNew: <boolean> (optional, default: false),
  silent: <boolean> (optional, default: false),
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false)
  checkGraphs: <boolean> (optional, default: false),
  graphName: <string> (optional),
}
```

For `/remove`:
```
{
  waitForSync: <boolean> (optional, default: false),
  returnOld: <boolean> (optional, default: false),
  silent: <boolean> (optional, default: false),
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false)
  checkGraphs: <boolean> (optional, default: false),
  graphName: <string> (optional),
}
```

For `/upsert`:
```
{
  waitForSync: <boolean> (optional, default: false),
  returnOld: <boolean> (optional, default: false),
  returnNew: <boolean> (optional, default: false),
  silent: <boolean> (optional, default: false),
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false)
  checkGraphs: <boolean> (optional, default: false),
  graphName: <string> (optional),
}
```

For `/repsert`:
```
{
  waitForSync: <boolean> (optional, default: false),
  returnOld: <boolean> (optional, default: false),
  returnNew: <boolean> (optional, default: false),
  silent: <boolean> (optional, default: false),
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false)
  checkGraphs: <boolean> (optional, default: false),
  graphName: <string> (optional),
}
```

## Details

### Details on the `data` objects:

`pattern` is always used as a search pattern, i.e., it looks for a document that
[`MATCHES()`](../../AQL/Functions/Document.md#matches) the pattern. If no such
document exists a _document not found_ error is returned. Except for `upsert`
and `repsert`, the attribute `pattern._key` is obligatory. Adding more
attributes than `_key` is useful for additional checks (e.g. `_rev`), as well as
specifying shard keys (for faster lookups).

### Details on the `options`:

With `checkGraphs` / `graphName` the operation can be modified to behave
as in the [Gharial API](../../HTTP/Gharial/README.md). That is, when
`graphName` is specified:

- collections that aren't part of the graph are rejected in all operations
  
Also, `checkGraphs` must be `true` when `graphName` is set.

When `checkGraphs` is true, the following things happen:

- when edges are inserted or modified and there is a corresponding edge
  definition, the incident vertices must match this definition
- when vertices are removed, incident edges are removed as well

When `singleTransaction` is `true`, all operations are executed in a single
transaction (which may be rolled back if any operation fails). Otherwise,
every operation is executed in its own transaction. The whole operation is
not aborted but continues even when a single operation fails. Setting
`singleTransaction: true` is not supported in a cluster setup, and as
`true` is the default, `singleTransaction: false` must be set.

The other options, namely `waitForSync`, `returnOld`, `returnNew`, and `silent`,
are as in the original [Documents API](../../HTTP/Document/README.md).

### Details on the response object:

```
{
  error: <boolean>,
  errorNum: <int>, (optional)
  errorMessage: <string>, (optional)
  errorDataIndex: <int>, (optional)
  results: <array>, (optional)
  ...
}
```

If an error occured, `error` is `true`. If a global error occured - that
includes any error that causes the whole transaction (with
`singleTransaction: true`) to fail - `errorNum` and `errorMessage` will be set
to contain the corresponding error information. If the error was caused by a
specific document, the (zero-based) index of the offending document in the
`data` array will be contained in `errorDataIndex`.

If the operation returns results, the response will contain a `result` array
with the same size as data.

If `singleTransaction` is `false`, and the operation failed for at least
one document, the response will also contain a `result` array with the same size as
data.

The `result` array may be omitted, if
- a global error occurred, or
- no result was requested (e.g. `silent: true`) *and* no error occurred.

The result entries have the form
```
{
  errorNum: <int>, (optional)
  errorMessage: <string>, (optional)
  ...
}
```
If `errorNum` is omitted it should be treated as `0`. If `errorMessage`
is omitted it should be treated as `""`. A result entry is successful if
`errorNum` is `0`, and therefore may be encoded as `{}` when there is no other
data to return. `errorMessage` will always be `""` (or omitted) when `errorNum`
is `0`.

#### Results contain the following additional data per route

For `/read`:

`result[i].doc` containing the fetched document.

For `/insert`:

- If `returnNew: false`: `result[i].new = {_key, _id, _rev}`
- If `returnNew: true`: `result[i].new` contains the new document
- If `silent: true`: nothing

For `/update`:

- If `returnNew: false`: `result[i].new = {_key, _id, _rev}`
- If `returnNew: true`: `result[i].new` contains the new document
- If `returnOld: true`: `result[i].old` contains the old document
- If `silent: true`: nothing

For `/replace`:

- If `returnNew: false`: `result[i].new = {_key, _id, _rev}`
- If `returnNew: true`: `result[i].new` contains the new document
- If `returnOld: true`: `result[i].old` contains the old document
- If `silent: true`: nothing

For `/remove`:

- If `returnOld: false`: `result[i].new = {_key, _id, _rev}`
- If `returnOld: true`: `result[i].old` contains the old document
- If `silent: true`: nothing

For `/upsert`:

- `inserted: <boolean>` is `true` if a new document was inserted, and false if
  an updated occurred.  
- If `returnNew: false`: `result[i].new = {_key, _id, _rev}`
- If `returnNew: true`: `result[i].new` contains the new document
- If `returnOld: true`: `result[i].old` contains the old document
- If `silent: true`: nothing

For `/repsert`:

- `inserted: <boolean>` is `true` if a new document was inserted, and false if
  a replace occurred.
- If `returnNew: false`: `result[i].new = {_key, _id, _rev}`
- If `returnNew: true`: `result[i].new` contains the new document
- If `returnOld: true`: `result[i].old` contains the old document
- If `silent: true`: nothing

## Justification

### HTTP methods and route names

Considerations:
- Its impractical to use `GET` to read batches, as the `GET` request body must
  be ignored.
- Besides `GET` and `HEAD`, only `POST` is allowed to be cached (if the server
  supplies appropriate Cache-Control or Expires header fields), so may be the
  most suitable replacement for `GET` in reading
- Upsert and Repsert can't be distinguished by HTTP method only from
  update/replace.
- The whole batch idea does not match REST very well, so we may need to differ.

So, to keep it simple and consistent, we use only `POST` and add the verb to the
route instead.

### Request body format

It's made to be easily extensible and as consistent as possible over the
different operations. All options are placed in `request.options` to keep it
simple: no options in the HTTP header or query parameters. Not allowing
`pattern` to be a string (which would be taken as `_key`) instead of an object
allows us, for example, to maybe later lift the restriction of passing `_key`
when another unique index is used. 

Never silently ignoring unknown attributes helps not only avoiding typos in the
client side, but also allows adding attributes later in a backwards-compatible
way.

#### New options

The single transaction option is defined somewhat inconvenient:
```
  singleTransaction: <boolean> | on single server: (optional, default: true)
                               | on cluster: (required to be set to false)
```

The reason behind this is, that
- it would be very impolite to take an option `singleTransaction: true` but
  silently ignore it and not use a transaction
- the default `true` seems very sane

Therefore, the best choice seems to be to force the user to explicitly and thus
knowingly disable transaction in the cluster.

## Possible minor changes to options

### `checkGraphs` and `graphName`

`checkGraphs` is to allow for normal document operations to be checked to not
break any graphs, while `graphName` restricts it further to a single graph. I
have in mind to use a single option instead for this, e.g.
```
  graph: <string|boolean> (optional, default: false)
```
, which would behave like this:

- if `false`, like `checkGraphs: false` and `graphName` unset
- if `true`, like `checkGraphs: true` and `graphName` unset
- if a string `str`, like `checkGraphs: true` and `graphName: str`

This would avoid invalid option combinations by design (like specifying a graph
name but setting `checkGraphs: false`).

### `silent`, `returnOld` and `returnNew`

I kept those for consistency with the old APIs in the proposal, but would really
like to change them in a way that avoids invalid or nonsensical options
combinations. For example, they could be replaced with

```
  replyWith: {
    new: <"off"|"meta"|"full"> (default: "meta"),
    old: <"off"|"meta"|"full"> (default: "none")
  }
```

This would not only avoid things like `{silent: true, returnNew: true}`, but
also allow to ask for the metadata of the old document without getting the whole
document (may not be useful very often, but still).


<!-- arangod/RestHandler/RestBatchDocumentHandler.cpp -->
@startDocuBlock batch_read_document

<!-- arangod/RestHandler/RestBatchDocumentHandler.cpp -->
@startDocuBlock batch_insert_document

<!-- arangod/RestHandler/RestBatchDocumentHandler.cpp -->
@startDocuBlock batch_update_document

<!-- arangod/RestHandler/RestBatchDocumentHandler.cpp -->
@startDocuBlock batch_replace_document

<!-- arangod/RestHandler/RestBatchDocumentHandler.cpp -->
@startDocuBlock batch_remove_document

<!-- arangod/RestHandler/RestBatchDocumentHandler.cpp -->
@startDocuBlock batch_upsert_document

<!-- arangod/RestHandler/RestBatchDocumentHandler.cpp -->
@startDocuBlock batch_repsert_document
