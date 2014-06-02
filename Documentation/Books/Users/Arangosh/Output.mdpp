!CHAPTER ArangoDB Shell Output

In general the ArangoDB shell prints its output to the standard output channel
using the JSON stringifier.

    arangosh> db.five.toArray();
    [{ "_id" : "five/3665447", "_rev" : "3665447", "name" : "one" }, 
    { "_id" : "five/3730983", "_rev" : "3730983", "name" : "two" }, 
    { "_id" : "five/3862055", "_rev" : "3862055", "name" : "four" }, 
    { "_id" : "five/3993127", "_rev" : "3993127", "name" : "three" }]

*start_pretty_print()*

While the standard JSON stringifier is very concise it is hard to read. Calling
the function *start_pretty_print* will enable the pretty printer which
formats the output in a human-readable way.

    arangosh> start_pretty_print();
    using pretty printing
    arangosh> db.five.toArray();
    [
      { 
        "_id" : "five/3665447", 
        "_rev" : "3665447", 
        "name" : "one"
      }, 
      { 
        "_id" : "five/3730983", 
        "_rev" : "3730983", 
        "name" : "two"
      }, 
      { 
        "_id" : "five/3862055", 
        "_rev" : "3862055", 
        "name" : "four"
      }, 
      { 
        "_id" : "five/3993127", 
        "_rev" : "3993127", 
        "name" : "three"
      }
    ]

*stop_pretty_print()*

The function disables the pretty printer, switching back to the standard dense
JSON output format.