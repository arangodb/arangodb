# VPackDeserializer library

This library helps to write deserializer. The generated deserializer does all required error checking and
produces meaningful error messages. 
The author of this lib tried hard to generate the _best deserializer possible_ in most common cases.

To understand how this library is used, lets deserialize a Agency Transaction.
```json
[
  [
    {"/a/b/c":  {"op": "set", "new":  12}},
    {"/a/d":  {"isEqual":  1}},
    "client-id-aaaa-bbbb-ccc-dd-cccc"
  ]
]
```

However, we want to represent a transaction internally as follows:
```cpp
using operation_list = vector_map<std::string_view, operation>;
using precondition_list = vector_map<std::string_view, precondition>; 

struct write_transaction {
  operation_list operations;
  precondition_list preconditions;
  std::string client_id;
};

using write_envelope = std::vector<write_transaction>;
```
An envelope consists of multiple write transactions. Thus we define the deserializer for an envelope as
```
template<typename T>
using default_vector = std::vector<T>; // required because std::vector is a template having multiple parameters

using write_envelope_deserializer = 
    deserializer::array_deserializer<write_transaction_deserializer, default_vector>;
```
i.e. we deserialize something as an array, where each entry is deserialized by a `write_transaction_deserializer`
and everything is put into a `std::vector`.
```
using write_transaction_deserializer = deserializer::utilities::constructing_deserializer<
    write_transaction,
    deserializer::fixed_order_deserializer<
        operation_deserializer,
        precondition_deserializer,
        value_deserializer<std::string>
    >>;
```
The write transaction is deserialized by a `constructing_deserialized` which (surprise) constructs the given type
(`write_transaction`) by brace-initialisation using as parameters a tuple returned by the previous stage.
This stage is a `fixed_order_deserializer` which deserializes an array in fixed order and returns it as tuple. 
In this case the first entry is deserialized as `operation_deserializer`, the second as 
`precondition_deserializer` and the  third entry is a simple `value_deserializer<std::string>`.
(Please compare this with the agency format explained above)

And then you define `operation_deserializer` and `precondition_deserializer`.

Finally you can deserialize a `VPackSlice` as follows:
```
auto env = deserializer::deserialize<write_envelope_deserializer>(slice);
```
`env` is a `result` object that either contains the deserialized object or an error containing an error message.

## More Examples
See tests here: `tests/VPackDeserializer/BasicTests.cpp`.
