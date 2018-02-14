Incompatible changes in ArangoDB 3.3
====================================

It is recommended to check the following list of incompatible changes **before**
upgrading to ArangoDB 3.4, and adjust any client programs if necessary.

The following incompatible changes have been made in ArangoDB 3.4:


Client tools
------------


REST API
--------

- GET /_api/aqlfunction/ was migrated to match the general structure of ArangoDB Replies. 
    It now returns an object with a "result" attribute that contains the list of available AQL user functions: 
```js
	  {
	    code: 200,
		error: false,
		result: [
	      {
		    "name":"UnitTests::mytest1",
			"code":"function () { return 1; }",
			"isDeterministic":false
		}]}
```
    In previous versions, this REST API returned only the list of available AQL user functions on the top level of the response.
	Each AQL user function description now also contains the 'isDeterministic' attribute.
- DELETE /_api/aqlfunction/ now returns the number of deleted functions:
	  `{code: 200, error: false, deletedCount: 10} `

Miscellaneous
-------------
