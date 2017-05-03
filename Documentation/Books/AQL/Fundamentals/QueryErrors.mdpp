Errors
======

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

Please refer to the [Arango Errors](../../Manual/Appendix/ErrorCodes.html) page
for a list of error codes and meanings.

