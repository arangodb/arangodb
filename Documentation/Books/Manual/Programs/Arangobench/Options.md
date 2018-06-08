Arangobench Startup Options
===========================

Usage: `arangobench [<options>]`

@startDocuBlock program_options_arangobench

Notes
-----

### Test cases

Value               | Description
--------------------|-------------------------
`aqlinsert`         | Insert documents via AQL
`aqltrx`            | AQL Transactions with deep nested AQL `FOR` - loops 
`aqlv8`             | Execute AQL with V8 functions to insert random documents
`collection`        | Creates collections
`counttrx`          | Uses JS transactions to count the documents and insert the result again
`crud`              | Create/Read/Update/Delete
`crud-append`       | Create/Read/Update/Read again
`crud-write-read`   | Create/Read Documents
`document`          | Creates documents
`edge`              | Create/Read/Update edge documents
`hash`              | Create/Read/Update/Read documents indexed by a hash index
`import-document`   | Creates documents via the import API
`multi-collection`  | Multiple transactions combining reads & writes from js on multiple collections
`multitrx`          | Multiple transactions combining reads & writes from js
`random-shapes`     | Create/Read/Delete heterogeneous documents with random values
`shapes`            | Create & Delete documents with heterogeneous attribute names
`shapes-append`     | Create documents with heterogeneous attribute names
`skiplist`          | Create/Read/Update/Read documents indexed by a skiplist
`stream-cursor`     | Create documents and retrieve them in a streaming fashion
`version`           | Requests /_api/version
