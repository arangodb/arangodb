The ArangoDB Shell {#UserManualArangosh}
========================================

@NAVIGATE_UserManualArangosh
@EMBEDTOC{UserManualArangoshTOC}

ArangoDB Shell Output {#UserManualArangoshOutput}
=================================================

In general the ArangoDB shell prints its as output to standard output channel
using the JSON stringifier.

    arangosh> db.five.all().toArray();
    [{ _id : "2223655/3665447", _rev : "3665447", name : "one" }, 
    { _id : "2223655/3730983", _rev : "3730983", name : "two" }, 
    { _id : "2223655/3862055", _rev : "3862055", name : "four" }, 
    { _id : "2223655/3993127", _rev : "3993127", name : "three" }]

@CLEARPAGE
@FUN{start_pretty_print()}

While the standard JSON stringifier is very concise it is hard to read.  Calling
the function @FN{start_pretty_print} will enable the pretty printer which
formats the output in a human readable way.

    arangosh> start_pretty_print();
    using pretty printing
    arangosh> db.five.all().toArray();
    [
      { 
	_id : "2223655/3665447", 
	_rev : "3665447", 
	name : "one"
       }, 
      { 
	_id : "2223655/3730983", 
	_rev : "3730983", 
	name : "two"
       }, 
      { 
	_id : "2223655/3862055", 
	_rev : "3862055", 
	name : "four"
       }, 
      { 
	_id : "2223655/3993127", 
	_rev : "3993127", 
	name : "three"
       }
    ]

@CLEARPAGE
@FUN{stop_pretty_print()}

The functions disable the pretty printer, switching back to the standard JSON
output format.
