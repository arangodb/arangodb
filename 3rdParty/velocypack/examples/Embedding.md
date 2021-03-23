Embedder info for the VPack library
===================================

Minimal example
---------------

Let's start with a small example program *test.cpp* that uses the VPack library.
It does nothing yet, the only goal is to make this minimal example compile and
link:

```cpp
#define VELOCYPACK_XXHASH 1

#include <velocypack/vpack.h>
#include <iostream>

using namespace arangodb::velocypack;

int main () {
  std::cout << valueTypeName(ValueType::Object) << std::endl;
}
```

To make the VPack classes available in your project, add the VPack headers
to the list of include directories. How exactly this works is compiler-specific.
For example, when using g++ or clang, include directories can be added using 
the `-I` compiler option.

When compiling the program, please make sure the compiler can understand C++11
syntax. In g++ and clang, this can be controlled via the `-std=c++11` option.

Additionally, the velocypack library must be linked to the example program.
In g++ this works by specifying the libary path with the `-L` option and specifying
the library's name.

The full instruction to compile and link the test program with g++ is:

```bash
g++ -std=c++11 -I/usr/local/include -L/usr/local/lib  main.cpp -lvelocypack -o test
```

With clang, it is:

```bash
clang++ -std=c++11 -I/usr/local/include -L/usr/local/lib  main.cpp -lvelocypack -o test
```

The test program can afterwards be run with

```bash
./test
```

With a working infrastructure for compiling and linking the VPack library,
you can now go ahead and adjust the example program so it does something useful. 
There are some working usage examples in this directory.


Exceptions and error reporting
------------------------------

The VPack library's way of signaling errors is to throw exceptions. Thus VPack
library users need to make sure they handle exceptions properly.

The VPack library will mostly throw exceptions of type `arangodb::vpack::Exception`. 
The library's `Exception` class is derived from `std::exception` and provides the 
`what` method to retrieve the error message from the exception.

Additionally, `Exception` provides the `errorCode` method for retrieving a
numeric error code from an exception. This error code can be used to check for
specific error types programmatically.

```cpp
Builder b;
b.add(Value(ValueType::Object));
try {
  // will fail as we should rather add a key/value pair here
  b.add(Value(ValueType::Null));
}
catch (Exception const& ex) {
 std::cout << "caught exception w/ code " << ex.errorCode()
           << ", msg: " << ex.what() << std::endl;
}
```

Additionally, the VPack library may throw standard exceptions such as
`std::bad_alloc` when appropriate. 

If no special treatment of VPack exceptions is needed by the client 
application, it is sufficient to only catch exceptions of type `std::exception`,
as the VPack `Exception` class is derived from it:

Builder b;
b.add(Value(ValueType::Object));
try {
  // will fail as we should rather add a key/value pair here
  b.add(Value(ValueType::Null));
}
catch (std::exception const& ex) {
 std::cout << "caught exception: " << ex.what() << std::endl;
}


Thread safety
-------------

Thread-safety was no design goal for VPack, so objects in the VPack library
are not thread-safe. VPack objects can be passed between threads though,
but if the same object is accessed concurrently, client applications need
to make sure they employ an appropriate locking mechanism on top.


Memory ownership and memory leaks
---------------------------------

In most cases there is no need to deal with raw memory when working with
VPack. By default, all VPack objects manage their own memory. This will also
avoid memory leaks.

It is encouraged to construct VPack objects on the stack rather than using
`new`/`delete`. This will greatly help avoiding memory leaks in client
code that uses the VPack library.

Special care must be taken for `Slice` objects: a `Slice` object contains a
pointer to memory where a VPack value is stored, and the client code needs
to make sure this memory is still valid when the `Slice` is accessed.

Here is a valid example for using a `Slice`:

```cpp
{
  Builder b;
  b.add(Value("this is a test"));

  // this Slice object is referencing memory owned by the Builder b
  // this works here as b is still available
  Slice s(b.start());

  // do something with Slice s in this scope...
}
```

Here is an invalid usage example, returning a `Slice` object that will
point to deallocated memory:

```cpp
Slice getSlice () {
  Builder b;
  b.add(Value("this is a test"));

  // this Slice object is referencing memory owned by the Builder b
  // this works here as b is still available
  Slice s(b.start());

  // the following return statement will make the Builder b go out of
  // scope. this will deallocate the memory owned by b, and accessing the
  // Slice s that points to b's memory will result in undefined behavior

  return s; // will return a Slice pointing to deallocated memory !!
}
```

In the latter case it would have been better to return the `Builder`
object from the function and not the `Slice`.

VPack `Buffer` objects also manage their own memory. When a `Buffer`
object goes out of scope, it will deallocate any dynamic memory it has
allocated. Client-code must not access the `Buffer` object's memory
after that.


String handling
---------------

The recommended way to get the contents of a String VPack value is to
use the method `Slice::copyString()`, which returns the String value in
an `std::string`. This is safe and convenient.

If access to a VPack String value's underlying `char const*` is needed for
performance reasons, then `Slice::getString()` will also work. Please be
careful when using it because VPack String values are not terminated with a 
NUL-byte as regular C string values. Using the returned `char const*` pointer
in functions that work on NUL-byte-terminated C strings will therefore
likely cause problems (crashes, undefined behavior etc.). In order to
avoid some of these problems, `Slice::getString()` also returns the length
of the String value in bytes in its first call argument:

```cpp
Slice slice = object.get("name"); 

if (slice.isString()) {
  // the following is unsafe, because the char* returned by
  // getString is not terminated with a NUL-byte
  ValueLength length;
  std::cout << "name* " << slice.getString(length) << std::endl;
  
  // better do this:
  char const* p = slice.getString(length);
  std::cout << "name* " << std::string(p, length) << std::endl;

  // or even better: 
  std::cout << "name: " << slice.copyString() << std::endl;
}
```


Including the VPack headers
---------------------------

The easiest way of making the VPack library's classes available to a client
program is to include the header `velocypack/vpack.h`. This will import all
class declarations from the namespace `arangodb::velocypack`. It is also possible 
to selectively include the headers for just the classes needed, e.g.

```cpp
// only need Builder and Slice in the following code
// no need to include all VPack classes via #include <velocypack/vpack.h>
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
```

Because only the actually required headers will be included, this variant may 
save some compilation time.

Name clashes and class aliases
------------------------------

To avoid full name qualification in client programs, it may be convenient to
make all classes from this namespace available without extra qualification.
The following line will do that:

```cpp
using namespace arangodb::velocypack`
```

However, this can lead to name clashes in the client application. For example,
the VPack library contains classes named `Buffer`, `Exception`, `Parser` -
class names which are not uncommon in many projects.

If for this reason importing the whole `arangodb::velocypack` namespace is 
not an option, an alternative is to use the class name aliases that are
defined in the header file `velocypack/velocypack-aliases.h`. 

This header file makes the most common VPack classes available under alternative 
(hopefully unambiguous) class names with the prefix *VPack*, for example:

```cpp
using VPackArrayIterator      = arangodb::velocypack::ArrayIterator;
using VPackBuilder            = arangodb::velocypack::Builder;
using VPackCharBuffer         = arangodb::velocypack::CharBuffer;
using VPackCharBufferSink     = arangodb::velocypack::CharBufferSink;
...
```

Note that the `velocypack-aliases.h` header will only make those VPack classes
available under alternative names that have been included already. When using
this header, it should be included after the other VPack headers have been
included:

```cpp
#include <velocypack/vpack.h>
#include <velocypack/velocypack-aliases.h>
```

or, when using selective headers:

```cpp
#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
```

Please check the file [exampleAliases.cpp](exampleAliases.cpp)
for a working example.

Picking a hash function
-----------------------

Velocypack can use a custom hash function for hashing Slice values. Hashes
of Slices will be used when using Slices as keys in associative STL containers,
or more generally, when comparing the contents of two Slice objects. 

By default VelocyPack comes with two hash functions that can be chosen from,
with the default hash function being xxhash.

The hash function can be changed at compile time by defining `VELOCYPACK_HASH`
before including the VelocyPack headers. The define must evaluate to a function
with three parameters:

- void const*: pointer to Slice contents
- size_t: length of Slice contents in bytes
- unsigned long long: initial seed for hash function

`VELOCYPACK_HASH` will be defined by Slice.h to the following macro when undefined: 
``` 
#define VELOCYPACK_HASH(mem, size, seed) XXH64(mem, size, seed)
```

Note that when changing the hash function via the `#define` it will be necessary
to re-compile the VelocyPack library and relink your program to it.
