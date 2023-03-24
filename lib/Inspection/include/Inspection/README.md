# Type inspection

At the moment this library is rather tightly coupled with VelocyPack since the
primary goal was to simplify (de)serialization from and to VelocyPack. It is
planned to reduce this coupling so that the Inspector concept can also be used
in other scenarios (e.g., deserializing from agency nodes).

## Acknowledgements

This library is heavily inspired by the type inspection library from the C++
Actor Framework (CAF). Kudos to Dominik Charousset & Co for their great work!

## Data Model

The current data model is largely based on the types supported by VelocyPack.

- built-in types
  double, boolean and string
  signed and unsigned integer types for 8, 16, 32 and 64 bit
- lists
  Dynamically-sized container types such as std::vector, std::list, etc.
- tuples
  Fixed-sized container types such as std::tuple, std::array or built-in C
  array types.
- maps
  Dynamically-sized container types with key/value pairs such as std::map.
  At the moment we only support strings as keys.
- objects
  User-defined types. An object has one or more fields. Fields have a name
  and may be optional. Further, fields may take on a fixed number of different
  types.  

There is also support for `std::optional`, `std::unique_ptr`, `std::shared_ptr`,
`velocypack::SharedSlice` and `std::variant`, as well as "unsafe" types like
`std::string_view` and `velocypack::Slice`, which we will cover later.

### Limitations

At the moment all types used for deserialization must be default constructible.

## Inspecting Objects

The inspection API allows to describe C++ objects. Users can either provide
free functions named `inspect` that are picked up via
[ADL](https://en.wikipedia.org/wiki/Argument-dependent_name_lookup) or
specialize `arangodb::inspection::Access`.

In both cases, users call member functions on an `Inspector` that provides a
domain-specific language (DSL) to describe the data (e.g., the structure of a
C++ object, the values of an enum, the alternatives of a `std::variant` etc).
Different inspectors can then process theses descriptions to do different
things. At the moment there are only two inspectors to allow (de)serializing
from/to velocypack, but the concept is very generic and allows us to add more
inspectors for other cases.

### Writing `inspect` Overloads

Adding overloads for `inspect` generally provides the simplest way since it
requires the least amount of boilerplate code.

For example consider this simple struct, with an inspect function where we
simply pass all member variables as fields to the inspector:

```cpp
struct VertexDescription {
  std::string_view id;
  uint64_t depth;
  double weight;
};

template<class Inspector>
auto inspect(Inspector& f, VertexDescription& description) {
  return f.object(description)
      .fields(f.field("vId", description.id),
              f.field("depth", description.depth),
              f.field("weight", description.weight));
}
```

Objects are simply containers for fields which in turn contain values. By
providing an `inspect` overload, we recursively traverses all fields and
"inspect" them as well.

Not every type needs to expose itself as `object`, though. A good example are
our internal identifiers (everything that derives from `basics::Identifier`):

```cpp
class Identifier {
  ...
  std::uint64_t id;
};
```

The type `Identifier` is basically a _strong typedef_ to improve type safety
when writing code. But if we describe it as an object with a field, then to an
inspector it looks as follows:

```
  object(type: "Identifier") {
    field(name: "id") {
      value(type: "uint64_t") {
        ...
      }
    }
  }
```

Now, this type has little use on its own and only introduces unnecessary
overhead. We do not need the additional object wrapper and could make it
completely transparent to inspectors. This can be achieved by writing the
`inspect` overload for `Identifier` as follows:

```cpp
  template <class Inspector>
  auto inspect(Inspector& f, Identifier& x) {
    return f.apply(x.id);
  }
```

### Types with Getter and Setter Access

This is currently not implemented. Please let me know in case you need support
for this.

### Fallbacks, Invariants and custom Context

For each field, we may provide a fallback value or fallback factory for optional
fields, as well a predicate that checks invariants on the data. For example,
consider the following class and its implementation for `inspect`:
```cpp
struct LogTargetConfig {
  std::size_t writeConcern = 1;
  std::size_t softWriteConcern = 1;
  bool waitForSync = false;
};

bool greaterZero(std::size_t v) { return v > 0; }

template<class Inspector>
auto inspect(Inspector& f, LogTargetConfig& x) {
  return f.object(x)
      .fields(f.field("writeConcern", x.writeConcern).invariant(greaterZero),
              f.field("softWriteConcern", x.softWriteConcern)
                  .fallback(std::ref(x.writeConcern)).invariant(greaterZero),
              f.field("waitForSync", x.waitForSync).fallback(f.keep()))
      .invariant([](LogTargetConfig& c) { return c.writeConcern >= c.softWriteConcern});
}
```

By default all attributes specified in the description have to be present when
loading. If an attribute is not present but has a fallback value (or factory)
defined, then the field is instead initialized with the fallback. Fallback
values are taken by value, but as the example shows we can use `std::ref` to
capture references. The attributes are processed in the same order they are
specified. `writeConcern` is a mandatory attribute while `softWriteConcern` is
optional, but if `softWriteConcern` is not specified explicitly, we want it to
default to the exact same value as `writeConcern`. Since `writeConcern` is
specified first, it is also processed first. For `softWriteConcern` we capture
a reference to `writeConcern` as fallback, so when we process `softWriteConcern`
and cannot find it, we take the fallback value from the already processed
`writeConcern`. Alternatively one can use `Inspector::keep()` to indicate that
we simply want to keep the current value of that field (see the fallback call
for "waitForSync" in the example).

`writeConcern` and `softWriteConcern` must both be greater zero, which is
verified by the provided invariant function. `invariant` takes some callable
that receives an argument of the same type as the field and must either return
a bool or `arangodb::inspection::Status` to indicated whether it was successful
or not. The advantage of using the `Status` type is that one can provide a
meaningful error message with more context.

In addition, `softWriteConcern` must not be greater than `writeConcern`.
Invariants like this can only be verified once all fields have been processed,
which is why you can append another `invariant` call to the result of `fields`.
This invariant function will then receive a reference to the fully initialized
object.

Invariants are only checked when `isLoading` is true (see "Splitting Save and
Load").

If all you want to do is check the invariants for a manually filled struct
there is the `ValidateInspector` that does just that. Since invariant are only
checked when `isLoading` is true, this is also set for the `ValidateInspector`,
even though it does _not_ modify the given object.

Being able to define fallback values and invariants is fine, but sometimes those
depend on some configuration that are not available in the `inspect` function.
This is were custom contexts come in. Suppose we want `writeConcern` to default
to some value that can be configured, e.g., via the command line. Then you can
pass an object that contains your `defaultWriteConcern` to the constructor of
the respective Inspector, which will store a reference to it. This reference is
then available via `getContext` as shown in the following example which uses a
`fallbackFactor` to initialize `writeConcern` (this is just so we also have an
example of how to use `fallbackFactory`; `fallback` would work just as well):
```cpp
template<class Inspector>
auto inspect(Inspector& f, LogTargetConfig& x) {
  auto& context = f.getContext();
  return f.object(x)
      .fields(f.field("writeConcern", x.writeConcern).invariant(greaterZero)
                  .fallbackFactory([&]() { return context.defaultWriteConcern; }),
              f.field("softWriteConcern", x.softWriteConcern)
                  .fallback(std::ref(x.writeConcern)).invariant(greaterZero),
              f.field("waitForSync", x.waitForSync).fallback(f.keep()))
      .invariant([](LogTargetConfig& c) { return c.writeConcern >= c.softWriteConcern});
}
```

### Embedded fields

In some cases we may want to "reuse" the inspect function of some type, e.g.,
in case of inheritance. This can be achieved using "field embedding":
```cpp
struct Inner {
  string s;
};
template<class Inspector>
auto inspect(Inspector& f, Inner& v) {
  return f.object(v).fields(f.field("s", v.s));
}

struct Base {
  int x;
};
template<class Inspector>
auto inspect(Inspector& f, Base& v) {
  return f.object(v).fields(f.field("x", v.x));
}

struct Derived : Base {
  int y;
  Inner i;
};
template<class Inspector>
auto inspect(Inspector& f, Derived& v) {
  return f.object(v).fields(
    f.embedFields(static_cast<Base&>(*this)),
    f.field("y", v.y),
    f.embedFields(v.i));
}
```
Here the `inspect` function for `Derived` embeds the fields for `Base` and
`Inner`, so we end up with an object description that looks like this:
```
  object(type: "Derived") {
    field(name: "x")
    field(name: "y")
    field(name: "s")
  }
```
A type that is used in `embedFields` must be inspected as an object, i.e., its
inspect function must use `object(..).fields(...)`. If this requirement is not
met the compiler should fail with a static_assert that points that out.

### Specializing `Access`

Instead of writing `inspect` functions one can specialize
`arangodb::inspection::Access`. This not only allows one to work with with 3rd
party libraries (for which adding free functions is usually ruled out), but
also allows to customize every step of the inspection process. It requires
writing more boilerplate code though.

The full interface of `Access` looks as follows:
```cpp
template <class T>
struct Access {
  template <class Inspector>
  static bool apply(Inspector& f, T& x);

  template<class Inspector>
  static auto saveField(Inspector& f, std::string_view name,
                        bool hasFallback, Value& val);

  template<class Inspector, class Transformer>
  static auto saveTransformedField(Inspector& f,
                                   std::string_view name,
                                   bool hasFallback, Value& val,
                                   Transformer& transformer);

  template<class Inspector>
  static Status loadField(Inspector& f, std::string_view name,
                          bool isPresent, Value& val);

  template<class Inspector, class ApplyFallback>
  static Status loadField(Inspector& f, std::string_view name,
                          bool isPresent, Value& val,
                          ApplyFallback&& applyFallback);

  template<class Inspector, class Transformer>
  static auto loadTransformedField(Inspector& f,
                                   std::string_view name,
                                   bool isPresent, Value& val,
                                   Transformer& transformer);

  template<class Inspector, class ApplyFallback, class Transformer>
  static Status loadTransformedField(
      Inspector& f, std::string_view name, bool isPresent, Value& val,
      ApplyFallback&& applyFallback, Transformer& transformer);
};
```

### Optionals

We previously saw how we can use `fallback` values for attributes that are not
mandatory. As an alternative to fallback values one can use optional values.
`std::optional`, `std::unique_ptr` and `std::shared_ptr` all qualify as optional
values. That is, if no matching attribute is found, then the field is set to
`std::nullopt`/`nullptr`. Otherwise, a default constructed instance of the
wrapped type is created and inspected recursively.

### Variants

Even though support for variants is built-in, the inspection library needs some
additional information about the variant type. There is an API to describe
variants - very similar to what we previously saw for objects. There are
different ways how a variant value can be encoded. There are inline types and
non-inline types. The latter have a dedicated type indicator field while the
first ones do not. Non-inline types can come in three different forms -
"qualified", "unqualified" and "embedded".

Consider the following example:
```cpp
using MyVariant = std::variant<std::string, int, Struct1> {};

template<class Inspector>
auto inspect(Inspector& f, MyVariant& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).qualified("type", "value").alternatives(
      insp::inlineType<std::string>(),
      insp::type<int>("int"),
      insp::type<Struct1>("Struct1"));
}
```
This serializes/deserializes the variant in "qualified form", where `string` is
defined as an inline type, while `int` and `Struct1` are non-inline types.

As already mentioned, inline types do not have a type indicator, but instead
the values are just directly stored as-is. So writing inline types is very
straightforward, but for parsing we somehow have to determine the type based
on the data. Basically how this is done is that we simply try to parse each of
the inline types (in the order in which they are specified). If the parse was
successful, then that is the type and value that we use, otherwise we continue
with the next type. So if you have two types `A` and `B` where `A` is a
supertype of `B` (i.e., every possible value for `B` would also be a possible
value for `A` - for example double/int), then `B` must be listed before `A`.

Note: _any_ errors (including failed invariants) that occur while trying to
parse inline types are _ignored_ and the type is dismissed!

Inline types are primarily useful for scalar types like `string`, `int` or
`bool`, but you can actually use it for arbitrary types. We try to be smart and
rule out some cases that we know will fail to parse based on the velocypack
type, e.g., if our target type is `string` and the velocypack type is something
else. But if none of these checks fail, we simply have to try to parse into an
instance of the current target type, so we have to create a default constructed
instance.

Inline types are always checked first (and therefore also _must_ be listed
before the non-inline types, otherwise you will get a compiler error pointing
this out). Only if none of those could be parsed we move on to the non-inline
types.

In "qualified" form the variant is serialized as an object with two attributes
as specified in the `qualified` call. For example:
```
{
  "type": "int",
  "value: 42
}
```
In "unqualified" form the variant is also serialized as an object, but only
uses a single attribute with the type name. Suppose we would write the
`inspect` function as follows:
```cpp
template<class Inspector>
auto inspect(Inspector& f, MyVariant& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).unqualified().alternatives(
      insp::type<std::string>("string"),
      insp::type<int>("int"),
      insp::type<Struct1>("Struct1"));
}
```
Then the generated result would instead look like this:
```
{
  "string": "foobar"
}
```
The "embedded" form can only be used if all types (except for the inline types)
in the variant are inspected as objects. In this form the type indicator field
is then serialized on the same level as the object fields:
```cpp
struct Struct1 { int a; }:
struct Struct2 { int b; }:
using MyEmbeddedVariant = std::variant<Struct1, Struct2> {};

template<class Inspector>
auto inspect(Inspector& f, MyEmbeddedVariant& x) {
  namespace insp = arangodb::inspection;
  return f.variant(x).embedded("type").alternatives(
      insp::type<Struct1>("Struct1"),
      insp::type<Struct2>("Struct2"));
}
```
If we were to serialize a `Struct2{.a = 42}` this would generate the following
result:
```
{
  "type": "Struct1",
  "a": 42
}
```

### Enums

There is a separate inspect API to define value mappings for enum types.
```cpp

enum class MyStringEnum {
  kValue1,
  kValue2,
  kValue3 = kValue2,
};

template<class Inspector>
auto inspect(Inspector& f, MyStringEnum& x) {
  return f.enumeration(x).values(MyStringEnum::kValue1, "value1",  //
                                 MyStringEnum::kValue2, "value2");
}
```
The call to values takes an arbitrary number of arguments, but is consumed
pairwise where the first value is the enum value, and the second one that value
that it is mapped to.

Enum values can be mapped to strings or integers. It is also possible to map one
enum value to multiple different strings and/or ints (i.e., you can mix the
target types). For example:
```cpp
enum class MyMixedEnum {
  kValue1,
  kValue2,
};

template<class Inspector>
auto inspect(Inspector& f, MyMixedEnum& x) {
  return f.enumeration(x).values(MyMixedEnum::kValue1, "value1",  //
                                 MyMixedEnum::kValue1, 1,         //
                                 MyMixedEnum::kValue2, "value2",  //
                                 MyMixedEnum::kValue2, 2);
}
```
In this case both enum values are mapped to both, a string and an integer value.
This means we can load both, string and int values and map them do our enum.
When saving, the _first_ mapping will be used, so in this example both values
would be saved as a string.

### Transformers

In some cases the we want to serialize the same type differently, e.g., in one
case we might want to serialize a `std::chrono::time_point` as ISO 8601 string,
while in another case we want a Unix timestamp. This can be achieved by using
transformers. A transformer is simply a type that provides an alias
`SerializedType` and two member functions `toSerialized` and `fromSerialized`.

For example see this dummy Transformer which converts between int and string:
```cpp
struct DummyTransformer {
  using SerializedType = std::string;

  arangodb::inspection::Status toSerialized(int v,
                                            SerializedType& result) const {
    result = std::to_string(v);
    return {};
  }
  arangodb::inspection::Status fromSerialized(SerializedType const& v,
                                              int& result) const {
    result = std::stoi(v);
    return {};
  }
};
```
The transformer is then applied to a field as follows:
```cpp
struct Foo {
  int x;
};

template<class Inspector>
auto inspect(Inspector& f, Foo& x) {
  return f.object(x).fields(f.field("x", x.x).transformWith(DummyTransformer{}));
}
```
So even though the field is an int, during serialization it will be transformed
into a string. During deserialization we convert back from string to int.

### Splitting Save and Load

Usually, load and save operations are symmetric, allowing us to provide a
single description of the data and leave the rest to the different inspectors.
But in some cases writing custom `inspect` functions with a single overload for
all inspectors may result in undesired tradeoffs or convoluted code. In these
cases it can be beneficial to split the code into separate `save` and `load`
parts. For this reason, all inspectors provide a static constant called
`isLoading`. This allows to use `if constexpr`, for example to delegate to
custom functions:

```cpp
template <class Inspector>
auto inspect(Inspector& f, my_class& x) {
  if constexpr (Inspector::isLoading) {
    return load(f, x);
  } else {
    return save(f, x);
  }
}
```

## Serializing / Deserializing

Writing the `inspect` functions is the prerequisite, but in order to actually
perform (de)serialization we have to create the according inspectors.
We currently have `VPackSaveInspector`, `VPackLoadInspector` and
`VPackUnsafeLoadInspector`. You have to use the last one in order to deserialize
unsafe types like `std::string_view` or `velocypack::Slice`. Those are
considered unsafe since they only store a pointer into the velocypack buffer
getting parsed, so if the parsed data outlives that buffer, we end up with
dangling pointers. Therefore, if you want to deserialize unsafe types, you have
to use the correct inspector and take care that all pointers remain valid.

To make things even easier, `Inspection/VPack.h` provides some free-standing
functions for serializing/deserializing to/from velocypack:
```cpp
namespace arangodb::velocypack {
  template<class T>
  void serialize(Builder& builder, T& value);

  template<class T>
  void deserialize(Slice slice, T& result,
                  inspection::ParseOptions options = {});

  template<class T>
  void deserializeUnsafe(Slice slice, T& result,
                        inspection::ParseOptions options = {});

  template<class T>
  T deserialize(Slice slice, inspection::ParseOptions options = {});

  template<class T>
  T deserializeUnsafe(Slice slice, inspection::ParseOptions options = {});
}  // namespace arangodb::velocypack
```
These functions will throw in case anything goes wrong. It is recommended to use
these functions instead of the according inspector types.

By default deserialization is very strict - the given velocypack must contain
_exactly_ those attributes specified in the object description. If an attribute
of non-optional type is missing and has no fallback, then this results in a
"missing required attribute "error. Likewise, any _additional_ attributes that
exist in the velocypack but not in the object results in a "unexpected attribute"
error. The `ParseOptions` provide a way to relax those checks and ignore unknown
and/or missing attributes. In case missing attributes are ignored, the
corresponding fields simply remain untouched and keep their original value.
