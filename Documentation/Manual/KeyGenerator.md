KeyGenerator {#GlossaryKeyGenerator}
================================================

@GE{Key Generator}: ArangoDB allows using key generators for each
collection. Key generators have the purpose of auto-generating 
values for the `_key` attribute of a document if none was specified
by the user. 

By default, ArangoDB will use the `traditional` key generator. 
The `traditional` key generator will auto-generate key values that 
are strings with ever-increasing numbers. The increment values it 
uses are non-deterministic.

Contrary, the `autoincrement` key generator will auto-generate
deterministic key values. Both the start value and the increment
value can be defined when the collection is created. The default
start value is 0 and the default increment is 1, meaning the key
values it will create by default are:

    1, 2, 3, 4, 5, ...

When creating a collection with the `autoincrement` key generator 
and an `increment` of `5`, the generated keys would be:

    1, 6, 11, 16, 21, ...
