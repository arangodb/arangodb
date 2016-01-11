

@brief finds an index

So you've created an index, and since its maintainance isn't for free,
you definitely want to know whether your query can utilize it.

You can use explain to verify whether **skiplists** or **hash indexes** are 
used (if you omit `colors: false` you will get nice colors in ArangoShell):

@EXAMPLE_ARANGOSH_OUTPUT{IndexVerify}
~db._create("example");
var explain = require("@arangodb/aql/explainer").explain;
db.example.ensureIndex({ type: "skiplist", fields: [ "a", "b" ] });
explain("FOR doc IN example FILTER doc.a < 23 RETURN doc", {colors:false});
~db._drop("example");
@END_EXAMPLE_ARANGOSH_OUTPUT

