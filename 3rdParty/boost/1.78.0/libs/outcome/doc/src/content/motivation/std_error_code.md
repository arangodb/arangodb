+++
title = "std::error_code"
description = "Overview of std::error_code"
weight = 40
+++


Type `std::error_code` has been designed to be sufficiently small and trivial
to be cheaply passed around, and at the same time be able to store sufficient
information to represent any error situation from any library/sub-system in the
world without a clash. Its representation is basically:

```c++
class error_code
{
  error_category* domain; // domain from which the error originates
  int             value;  // numeric value of error within the domain
};
```

Here, `domain` indicates the library from which the error originates. It is a
pointer to a global object representing a given library/domain. Different
libraries will be represented by different pointers to different globals.
Each domain is expected to be represented by a global object derived from
`std::error_category`. The uniqueness of the domain pointer value is guaranteed
by the uniqueness of addresses of different global objects.

Now, `value` represents a numeric value of a particular error situation within
the domain. Thus, different domains can use the same numeric value `1` to
indicate different error situations, but two `std::error_code` objects will be
different because the pointers representing domains will be different.

`std::error_code` comes with additional tools: a facility for defining custom
domains with their set of error codes, and a facility for building predicates
that allow classifying errors.

Once created and passed around (either inside a thrown exception or returned from functions by value) there is never a need to change the value of `error_code`
object at any level. But at different levels one can use different predicates
for classifying error situations appropriately to the program layer.

When a new library needs to represent its own set of error situations in an
`error_code` it first has to declare the list of numeric value as an enumeration:

```c++
enum class ConvertErrc
{
  StringTooLong = 1, // 0 should not represent an error
  EmptyString   = 2,
  IllegalChar   = 3,
};
```

Then it has to put some boiler-plate code to plug the new enumeration into the
`std::error_code` system. Then, it can use the enum as an `error_code`:

```c++
std::error_code ec = ConvertErrc::EmptyString;
assert(ec == ConvertErrc::EmptyString);
```

Member `value` is mapped directly from the numeric value in the enumeration, and
member `domain` is mapped from the type of the enumeration. Thus, this is a form
of type erasure, but one that does allow type `std::error_code` to be trivial
and standard-layout.
