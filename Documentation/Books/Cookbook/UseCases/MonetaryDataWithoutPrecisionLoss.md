Working with monetary data without precision loss in ArangoDB
=============================================================

Problem
-------

Applications that handle monetary data often require to capture fractional units
of currency and need to emulate decimal rounding without precision loss.
Compared to relational databases, JSON does not support arbitrary precision
out-of-the-box but there are suitable workarounds.

Solution
--------

In ArangoDB there are two ways to handle monetary data:

1. Monetary data **as integer**:
   <br>
   If you store data as integer, decimals can be avoided by using a general
   scale factor, eg. `100` making `19.99` to `1999`. This solution will work
   for digits of up to (excluding) 2<sup>53</sup> without precision loss. Calculations
   can then be done on the server side.
   
2. Monetary data **as string**:
   <br>
   If you only want to store and retrieve monetary data you can do so without
   any precision loss by storing this data as string. However, when using
   strings for monetary data values it will not be possible to do calculations
   on them on the server. Calculations have to happen in application logic
   that is capable of doing arithmetic on string-encoded integers.
   
 **Authors:**
 [Jan Stücke](https://github.com/MrPieces),
 [Jan Steemann](https://github.com/jsteemann)
 
 **Tags**: #howto #datamodel #numbers
 
