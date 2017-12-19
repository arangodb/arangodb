!CHAPTER Query results 

!SUBSECTION Result sets

The result of an AQL query is an array of values. The individual values in the
result array may or may not have a homogeneous structure, depending on what is
actually queried.

For example, when returning data from a collection with inhomogeneous documents
(the individual documents in the collection have different attribute names)
without modification, the result values will as well have an inhomogeneous
structure. Each result value itself is a document:

    FOR u IN users
      RETURN u
    
    [ { "id" : 1, "name" : "John", "active" : false }, 
      { "age" : 32, "id" : 2, "name" : "Vanessa" }, 
      { "friends" : [ "John", "Vanessa" ], "id" : 3, "name" : "Amy" } ]

However, if a fixed set of attributes from the collection is queried, then the 
query result values will have a homogeneous structure. Each result value is
still a document:

    FOR u IN users
      RETURN { "id" : u.id, "name" : u.name }
    
    [ { "id" : 1, "name" : "John" }, 
      { "id" : 2, "name" : "Vanessa" }, 
      { "id" : 3, "name" : "Amy" } ]

It is also possible to query just scalar values. In this case, the result set
is an array of scalars, and each result value is a scalar value:

    FOR u IN users
      RETURN u.id
    
    [ 1, 2, 3 ]

If a query does not produce any results because no matching data can be
found, it will produce an empty result array:

    [ ]

!SUBSECTION Errors

Issuing an invalid query to the server will result in a parse error if the query
is syntactically invalid. ArangoDB will detect such errors during query
inspection and abort further processing. Instead, the error number and an error
message are returned so that the errors can be fixed.

If a query passes the parsing stage, all collections referenced in the query
will be opened. If any of the referenced collections is not present, query
execution will again be aborted and an appropriate error message will be
returned.

Under some circumstances, executing a query may also produce run-time errors 
that cannot be predicted from inspecting the query text alone. This is because
queries may use data from collections that may also be inhomogeneous.  Some
examples that will cause run-time errors are:

- Division by zero: Will be triggered when an attempt is made to use the value
  *0* as the divisor in an arithmetic division or modulus operation
- Invalid operands for arithmetic operations: Will be triggered when an attempt
  is made to use any non-numeric values as operands in arithmetic operations.
  This includes unary (unary minus, unary plus) and binary operations (plus,
  minus, multiplication, division, and modulus)
- Invalid operands for logical operations: Will be triggered when an attempt is
  made to use any non-boolean values as operand(s) in logical operations. This
  includes unary (logical not/negation), binary (logical and, logical or), and
  the ternary operators

Please refer to the [Arango Errors](../ErrorCodes/README.md) page for a list of error codes and
meanings.

