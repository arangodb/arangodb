API documentation for the VPack library
=======================================

This document provides a step-by-step instruction for using the most 
important VPack classes. Use cases covered are 

* building new VPack objects dynamically
* inspecting the contents of VPack objects
* converting JSON data into VPack values
* serializing VPack values to JSON 

Please also have a look at the file [Embedding.md](Embedding.md) as it
contains information about how to include the VPack classes in other
applications and link against the VPack library.

All examples provided here and even more are available as separate files in 
the *examples* directory. These examples are also built and placed in the 
`build/examples/` directory when building VelocyPack with the default options.


Building VPack objects programmatically
---------------------------------------

See also: [exampleBuilder.cpp](exampleBuilder.cpp)

VPack objects can be assembled easily with a `Builder` object.
This `Builder` class organizes the buildup of one or many VPack objects.
It manages the memory allocation and provides convenience methods to build
compound objects recursively.

The following example program will build a top-level object of VPack 
type `Object`, with the following 4 keys:

- `b`: is an integer with value `12`
- `a`: is the Boolean value `true`
- `l`: is an Array with the 3 numeric sub-values `1`, `2` and `3`
- `name`: is the String value `"Gustav"`

This resembles the following JSON object:
```json
{
  "b" : 12,
  "a" : true,
  "l" : [
    1,
    2,
    3
  ],
  "name" : "Gustav"
}
```

After the VPack value is created, the example program will print a 
hex dump of its underlying memory:

```cpp
#include <velocypack/vpack.h>
#include <iostream>

using namespace arangodb::velocypack;

int main () {
  // create an object with attributes "b", "a", "l" and "name"
  // note that the attribute names will be sorted in the target VPack object!
  Builder b;

  b.add(Value(ValueType::Object));
  b.add("b", Value(12));
  b.add("a", Value(true));
  b.add("l", Value(ValueType::Array));
  b.add(Value(1));
  b.add(Value(2));
  b.add(Value(3));
  b.close();
  b.add("name", Value("Gustav"));
  b.close();

  // now dump the resulting VPack value
  std::cout << "Resulting VPack:" << std::endl;
  std::cout << HexDump(b.slice()) << std::endl;
}
```

Values will automatically be added to the last opened compound value, 
i.e. the last opened `Array` or `Object` value. To finish a compound
object, the Builder's `close()` method must be called.


See also: [exampleBuilderFancy.cpp](exampleBuilderFancy.cpp)

If you like fancy syntactic sugar, the same object can alternatively be
built using operator syntax:

```cpp
#include <velocypack/vpack.h>
#include <iostream>

using namespace arangodb::velocypack;

int main () {
  // create an object with attributes "b", "a", "l" and "name"
  // note that the attribute names will be sorted in the target VPack object!
  Builder b;

  b(Value(ValueType::Object))
    ("b", Value(12))
    ("a", Value(true))
    ("l", Value(ValueType::Array))
      (Value(1)) (Value(2)) (Value(3)) ()
    ("name", Value("Gustav")) ();

  // now dump the resulting VPack value
  std::cout << "Resulting VPack:" << std::endl;
  std::cout << HexDump(b.slice()) << std::endl;
}
```

Note that the behavior of `Builder` objects can be adjusted by setting the
following attributes in the Builder's `options` attribute:

- `sortAttributeNames`: when creating a VPack Object value, the Builder
  object will sort the Object's attribute names alphabetically in the
  assembled VPack object.
  Sorting the names allows accessing individual attributes by name quickly
  later. However, sorting takes CPU time so client applications may
  want to turn it off, especially when constructing *temporary*
  objects. Additionally, sorting may lead to the VPack Object having
  a different order of attributes than the source JSON value.
  By default, attribute names are sorted.
- `checkAttributeUniqueness`: when building a VPack Object value,
  the same attribute name may be used multiple times due to a programming
  error in the client application.
  Client applications can set this flag to make the `Builder` validate
  that attribute names are actually unique on each nesting level of Object
  values. This option is turned off by default to save CPU time.

For example, to turn on attribute name uniqueness checks and turn off
the attribute name sorting, a `Builder` could be configured as follows:

```cpp
Options options;
options.checkAttributeUniqueness = true;
options.sortAttributeNames = false;

Builder b(&options);

// now do something with Builder b
```


Inspecting the contents of a VPack object
-----------------------------------------

See also: [exampleSlice.cpp](exampleSlice.cpp),
[exampleObjectLookup.cpp](exampleObjectLookup.cpp)

The `Slice` class can be used for accessing existing VPack objects and
inspecting them. A `Slice` can be considered a *view over a memory
region that contains a VPack value*, but it provides high-level access
methods so users don't need to mess with the raw memory. 
`Slice` objects themselves are very lightweight and the cost of their
construction is relatively low.

`Slice` objects are immutable and cannot be used to modify the underlying 
VPack value. In order to modify an existing `VPack` value, a new value
needs to be assembled using a `Builder` or a `Parser` object.

It should be noted that `Slice` objects do not own the VPack value
they are pointing to. The client must make sure that the memory a `Slice`
refers to is actually valid. In the above case, the VPack value is
owned by the Builder object b, which is still available and valid when
Slice object s is created and used. 

To inspect the value contained in a Slice, it's best to call the Slice's
`type()` method first. Based on its return value, other actions can
follow. Slice also provides convenience methods for type checking such
as `isObject()`, `isArray()`, `isString()`, `isNumber()`, `isBool()` etc.

To access the value contents of a Slice, call the appropriate value getter
method, e.g.:

* `getBool()` for boolean values
* `getUInt()`, `getInt()`, `getDouble()` for retrieving the number value
  as uint64_t, int64_t or double, or `getNumber<T>` as a generic method
* `getString()` or `copyString()` for String values (note that you should better 
  use `copyString()` for reasons outlined in [Embedding.md](Embedding.md)

Slices of type `Array` and `Object` provide a method `length()` that
returns the number of Array / Object members. Objects also provide
`get()` and `hasKey()` to access members, Arrays provide `at()`.

To lookup a key in a Slice of type Object, there is the Slice's `get()`
method. It works by passing it a single key name as an `std::string` or by
passing it a vector of `std::strings` for recursive key lookups. `operator[]`
for a Slice will also call `get()`.

The existence of a key can be checked by using the Slice's `hasKey()`
method. Both methods should only be called on Slices that contain Object
values and will throw an Exception otherwise.

```cpp
#include <velocypack/vpack.h>
#include <iostream>

using namespace arangodb::velocypack;

int main () {
  // create an object with attributes "b", "a", "l" and "name"
  // note that the attribute names will be sorted in the target VPack object!
  Builder b;

  b(Value(ValueType::Object))
    ("b", Value(12))
    ("a", Value(true))
    ("l", Value(ValueType::Array))
      (Value(1)) (Value(2)) (Value(3)) ()
    ("name", Value("Gustav")) ();

  // a Slice is a lightweight accessor for a VPack value
  Slice s(b.start());

  // inspect the outermost value (should be an Object...)
  std::cout << "Slice: " << s << std::endl;
  std::cout << "Type: " << s.type() << std::endl;
  std::cout << "Bytesize: " << s.byteSize() << std::endl;
  std::cout << "Members: " << s.length() << std::endl;

  if (s.isObject()) {
    Slice ss = s.get("l");   // Now ss points to the subvalue under "l"
    std::cout << "Length of .l: " << ss.length() << std::endl;
    std::cout << "Second entry of .l:" << ss.at(1).getInt() << std::endl;
  }

  Slice sss = s.get("name");
  if (sss.isString()) {
    ValueLength len;
    char const* st = sss.getString(len);
    std::cout << "Name in .name: " << std::string(st, len) << std::endl;
  }
}
```

Here's an example working on nested Object values:

```cpp
// create an object with a few members
Builder b;

b(Value(ValueType::Object));
b.add("foo", Value(42)); 
b.add("bar", Value("some string value")); 
b.add("baz", Value(ValueType::Object));
b.add("qux", Value(true));
b.add("bart", Value("this is a string"));
b.close();
b.add("quux", Value(12345));
b.close();
  
Slice s(b.start());

// now fetch the string in the object's "bar" attribute
if (s.hasKey("bar")) {
  Slice bar(s.get("bar"));
  std::cout << "'bar' attribute value has type: " << bar.type() << std::endl;
}

// fetch non-existing attribute "quetzal"
Slice quetzal(s.get("quetzal"));
// note: this returns a slice of type None
std::cout << "'quetzal' attribute value has type: " << quetzal.type() << std::endl;
std::cout << "'quetzal' attribute is None: " << std::boolalpha << quetzal.isNone() << std::endl;

// fetch subattribute "baz.qux"
Slice qux(s.get(std::vector<std::string>({ "baz", "qux" })));
std::cout << "'baz'.'qux' attribute has type: " << qux.type() << std::endl;
std::cout << "'baz'.'qux' attribute has bool value: " << std::boolalpha << qux.getBoolean() << std::endl;
std::cout << "Complete value of 'baz' is: " << s.get("baz").toJson() << std::endl;

// fetch non-existing subattribute "bark.foobar" 
Slice foobar(s.get(std::vector<std::string>({ "bark", "foobar" })));
std::cout << "'bark'.'foobar' attribute is None: " << std::boolalpha << foobar.isNone() << std::endl;

// check if subattribute "baz"."bart" does exist
if (s.hasKey(std::vector<std::string>({ "baz", "bart" }))) {
  // access subattribute using operator syntax
  std::cout << "'baz'.'bart' attribute has type: " << s["baz"]["bart"].type() << std::endl;
  std::cout << "'baz'.'bart' attribute has value: '" << s["baz"]["bart"].copyString() << "'" << std::endl;
}
```

To retrieve the storage size of a Slice value (including any sub values)
there is the `byteSize()` method. Slice objects can easily be printed to 
JSON using the Slice's `toJson()` method. Slices can also be used in hashed 
containers as they implemented `std::hash` and `std::equal_to`. Retrieving 
the hash value for a Slice can be achieved by calling the Slice's `hash()` 
method. 


Iterating over VPack Arrays and Objects
---------------------------------------

See also: [exampleArrayIterator.cpp](exampleArrayIterator.cpp),
[exampleObjectIterator.cpp](exampleObjectIterator.cpp)

With the VPack compound value types *Array* and *Object* there comes the
need to iterate over their individual members. This can be achieved easily
with the helper classes `ArrayIterator` and `ObjectIterator`.

An `ArrayIterator` object can be constructed from a `Slice` with an 
underlying VPack Array value. Iterating over the Array members can then
be achieved easily using a range-based for loop:

```cpp
Builder b;

// create an Array value with 10 numeric members...
b(Value(ValueType::Array));
for (size_t i = 0; i < 10; ++i) {
  b.add(Value(i));
}
b.close();

Slice s(b.start());

// ...and iterate over the array's members
for (auto const& it : ArrayIterator(s)) {
  std::cout << it << ", number value: " << it.getUInt() << std::endl;
}
```

For VPack values of type *Object*, there is `ObjectIterator`. It provides
the attributes `key` and `value` for the currently pointed-to member: 

```cpp
Builder b;

// create an Object value...
b(Value(ValueType::Object));
b.add("foo", Value(42)); 
b.add("bar", Value("some string value")); 
b.add("qux", Value(true));
b.close();

Slice s(b.start());

// ...and iterate over its members
for (auto const& it : ObjectIterator(s)) {
  std::cout << it.key.copyString() << ", value: " << it.value << std::endl;
}
```


Working with Arrays and Objects
-------------------------------

See also: [exampleArrayHandling.cpp](exampleArrayHandling.cpp),
[exampleObjectHandling.cpp](exampleObjectHandling.cpp)

When working with Array values, a common task is to iterate over the Array's
members. The `Collection` class provides several static helper methods for this:

* `forEach()`: walks the Array's members one by one and calls a user-defined
  function for each. The current value and the member index are provided as 
  parameters to the user function

* `filter()`: runs a user-defined predicate function on all members and returns
  a new Array containing those members for which the predicate function returned
  true.

* `find()`: walks the Array's members one by one and calls a user-defined predicate
  function for each. Returns the member for which the predicate function returned 
  true, and a Slice of type None in case the predicate was always false. 

* `contains()`: same as find, but returns a boolean value and not the found Slice.

* `indexOf()`: same as find, but returns the index of the found Slice and 
  `Collection::NotFound` in case the sought Slice is not contained in the array.

* `all()`: walks the Array's members one by one and calls a user-defined predicate 
  function for each. Returns true if the predicate function returned true for all
  members, and false otherwise.

* `any()`: walks the Array's members one by one and calls a user-defined predicate 
  function for each. Returns true if the predicate function returned true for any of
  the Array members, and false otherwise.

The `Collection` class provides the following methods for working with `Object` values:

* `keys()`: returns the Object's keys as a vector strings or an unordered set
* `values()`: returns the Object's values as a new Array value
* `keep()`: returns a new Object value that contains only the mentioned keys
* `remove()`: returns a new Object value that contains all but the mentioned keys
* `merge()`: recursively merges two Object values
* `visitRecursive()`: recursively visits an Array and calls a user-defined predicate
  function for each visited value


Parsing JSON into a VPack value
-------------------------------

See also: [exampleParser.cpp](exampleParser.cpp)

Often there is already existing data in JSON format. To convert that
data into VPack, use the `Parser` class. `Parser` provides an efficient 
JSON parser.

A `Parser` object contains its own `Builder` object. After parsing this
`Builder` object will contain the VPack value constructed from the JSON
input. Use the Parser's `steal()` method to get your hands on that 
`Builder` object:

```cpp
#include <velocypack/vpack.h>
#include <iostream>

using namespace arangodb::velocypack;

int main () {
  // this is the JSON string we are going to parse
  std::string const json = "{\"a\":12,\"b\":\"foobar\"}";

  Parser parser;
  try {
    size_t nr = parser.parse(json);
    std::cout << "Number of values: " << nr << std::endl;
  }
  catch (std::bad_alloc const& e) {
    std::cout << "Out of memory!" << std::endl;
    throw;
  }
  catch (Exception const& e) {
    std::cout << "Parse error: " << e.what() << std::endl;
    std::cout << "Position of error: " << parser.errorPos() << std::endl;
    throw;
  }

  // the parser is done. now get its Builder object
  Builder b = parser.steal();

  // now dump the resulting VPack value
  std::cout << "Resulting VPack:" << std::endl;
  std::cout << HexDump(b.slice()) << std::endl;
}
```

When a parse error occurs while parsing the JSON input, the `Parser`
object will throw a VPack `Exception`. The exception message can be retrieved
by querying the Exception's `what()` method. The error position in the
input JSON can be retrieved by using the Parser's `errorPos()` method.

The parser behavior can be adjusted by setting the following attributes
in the Parser's `options` attribute:

- `validateUt8Strings`: when set to `true`, string values will be
  validated for UTF-8 compliance. UTF-8 checking can slow down the parser
  performance so client applications are given the choice about this.
  By default, UTF-8 checking is turned off.
- `sortAttributeNames`: when creating a VPack Object value
  programmatically or via a (JSON) Parser, the Builder object will
  sort the Object's attribute names alphabetically in the assembled
  VPack object.
  Sorting the names allows accessing individual attributes by name quickly
  later. However, sorting takes CPU time so client applications may
  want to turn it off, especially when constructing *temporary*
  objects. Additionally, sorting may lead to the VPack Object having
  a different order of attributes than the source JSON value.
  By default, attribute names are sorted.
- `checkAttributeUniqueness`: when building a VPack Object value,
  the same attribute name may be used multiple times due to a programming
  error in the client application or when parsing JSON input data with
  duplicate attribute names.
  For untrusted inputs, client applications can set this flag to make the
  `Builder` validate that attribute names are actually unique on each
  nesting level of Object values. This option is turned off by default to
  save CPU time.

Here's an example that configures the Parser to validate UTF-8 strings
in its JSON input. Additionally, the options for the Parser's `Builder`
object are adjusted so attribute name uniqueness is checked and attribute
name sorting is turned off:

```cpp
Options options;
options.validateUtf8Strings = true;
options.checkAttributeUniqueness = true;
options.sortAttributeNames = false;

Parser parser(&options);

// now do something with the parser
```


Serializing a VPack value into JSON
-----------------------------------

See also: [exampleDumper.cpp](exampleDumper.cpp),
[exampleDumperPretty.cpp](exampleDumperPretty.cpp)

When the task is to create a JSON representation of a VPack value, the
`Dumper` class can be used. A `Dumper` needs a `Sink` for writing the
data. There are ready-to-use `Sink`s for writing into a `char[]` buffer 
or into an `std::string` or an `std::ostringstream`.

```cpp
#include <iostream>
#include "velocypack/vpack.h"

using namespace arangodb::velocypack;

int main () {
  // don't sort the attribute names in the VPack object we construct
  // attribute name sorting is turned on by default, so attributes can
  // be quickly accessed by name. however, sorting adds overhead when
  // constructing VPack objects so it's optional. there may also be
  // cases when the original attribute order needs to be preserved. in
  // this case, turning off sorting will help, too
  Options options;
  options.sortAttributeNames = false;

  Builder b(&options);
  
  // build an object with attribute names "b", "a", "l", "name"
  b(Value(ValueType::Object))
    ("b", Value(12))
    ("a", Value(true))
    ("l", Value(ValueType::Array))
      (Value(1)) (Value(2)) (Value(3)) ()
    ("name", Value("Gustav")) ();

  Slice s(b.start());

  // turn on pretty printing
  Options dumperOptions;
  dumperOptions.prettyPrint = true;

  // now dump the Slice into an std::string
  std::string result;
  StringSink sink(&result);
  Dumper dumper(&sink, &dumperOptions);
  dumper.dump(s);

  // and print it
  std::cout << "Resulting JSON:" << std::endl << result << std::endl;
}
```

Note that VPack values may contain data types that cannot be represented in
JSON. If a value is dumped that has no equivalent in JSON, a `Dumper` object
will by default throw an exception. To change the default behavior, set the
Dumper's `unsupportedTypeBehavior` attribute to `NullifyUnsupportedType`:

```cpp
// convert non-JSON types into JSON null values if possible
Options options;
options.unsupportedTypeBehavior = NullifyUnsupportedType;

// now dump the Slice into an std::string
Dumper dumper(&sink, &options);
dumper.dump(s);
```

The options can also be used to control whether forward slashes inside strings 
should be escaped with a backslash when producing JSON. The default is to not 
escape them. This can be changed by setting the `escapeForwardSlashes` attribute 
in the options.

```cpp
Parser parser;
parser.parse("{\"foo\":\"this/is/a/test\"}");

Options options;
options.escapeForwardSlashes = true;

std::string result;
StringSink sink(&result);
Dumper dumper(&sink, &options);
dumper.dump(parser.steal().slice());
std::cout << result << std::endl;
```

Note that several JSON parsers in the wild provide extensions to the
original JSON format. For example, some implementations allow comments 
inside the JSON or support usage of the literals `inf` and `nan`/`NaN`
for out-of-range and invalid numbers. The VPack JSON parser does not 
support any of these extensions but sticks to the JSON specification.

