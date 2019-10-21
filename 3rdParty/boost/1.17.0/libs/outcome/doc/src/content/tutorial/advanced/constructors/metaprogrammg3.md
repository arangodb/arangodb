+++
title = "Alternatives"
description = ""
weight = 60
+++

No doubt many will dislike the two-stage invocation pattern i.e.

```c++
make<file_handle>{"hello"}();
```

So let us examine the most obvious alternative: a templated free function `make<T>`.

Due to the inability to partially specialise templated functions in C++, you
need to use tagged overloading e.g.

```c++
template<class... Args>
inline outcome::result<file_handle> make(std::in_place_type_t<file_handle>, Args&& ... args)
{
  return file_handle::file(std::forward<Args>(args)...);
}
...
// Now you must always write this:
make(std::in_place_type<file_handle>, "hello");
```

Tagged overloading is fine for smaller projects, but for larger code bases:

1. It takes longer to type `make(std::in_place_type<file_handle>, "hello")`,
and is possibly less intuitive to write,
than it does `make<file_handle>{"hello"}()`.
2. Compiler error messages are enormously clearer if you encode the permitted
overloads for construction into the `make<file_handle>` type rather than
letting a variadic free function fail to resolve an appropriate overload.
3. Resolving variadic free function overloads is not constant time for the compiler,
whereas resolving the type specialisation for `make<file_handle>`
is constant time. In other words, free functions are *expensive* on build
times, whereas fully specialised types are not.
4. It actually turns out to be quite useful when writing generic code
to pass around object constructing factory objects all of which have
no parameters for their call operator. It becomes, effectively, a 
*lazy construction* mechanism.