!CHAPTER Passing parameters to transactions

Arbitrary parameters can be passed to transactions by setting the *params* 
attribute when declaring the transaction. This feature is handy to re-use the
same transaction code for multiple calls but with different parameters.

A basic example:

    db._executeTransaction({
      collections: { },
      action: "function (params) { return params[1]; }",
      params: [ 1, 2, 3 ]
    });

The above example will return *2*.

Some example that uses collections:

    db._executeTransaction({
      collections: { 
        write: "users",
        read: [ "c1", "c2" ]
      },
      action: "function (params) { var db = require('internal').db; var doc = db.c1.document(params['c1Key']); db.users.save(doc); doc = db.c2.document(params['c2Key']); db.users.save(doc);}", 
      params: { 
        c1Key: "foo", 
        c2Key: "bar" 
      }
    });

!SUBSECTION Disallowed operations

Some operations are not allowed inside ArangoDB transactions:
- creation and deletion of collections (*db._create()*, *db._drop()*, *db._rename()*)
- creation and deletion of indexes (*db.ensure...Index()*, *db.dropIndex()*)

If an attempt is made to carry out any of these operations during a transaction,
ArangoDB will abort the transaction with error code *1653 (disallowed operation inside
transaction)*.

