+++
title = "Limitations"
description = ""
weight = 10
+++

C++ has excellent two-way compatibility with the C ABI, but there are some
limitations you must observe to write C++ code which C code can call without
marshalling at the ABI boundary:

1. A C++ function may not throw exceptions if it is safe to call from C, and
so should always be marked `noexcept`.

2. A C++ function should be annotated with `extern "C"` to prevent its symbol
being mangled, and thus give it the C rather than C++ ABI.

3. You cannot use overloading in your `extern "C"` functions.

4. You may only use types in your C++ function declaration for which these traits are both true:
  - [`std::is_standard_layout_v<T>`](http://en.cppreference.com/w/cpp/types/is_standard_layout)
  - [`std::is_trivially_copyable_v<T>`](http://en.cppreference.com/w/cpp/types/is_trivially_copyable)

    (Note that `std::is_trivially_copyable_v<T>` requires trivial destruction,
but NOT trivial construction. This means that C++ can do non-trivial construction
of otherwise trivial types)

---

The above is what the standard officially requires for *well defined* C and C++ interop.
However, all of the three major compilers MSVC, GCC and clang are considerably more relaxed.
In those three major compilers, "almost-standard-layout" C++ types work fine in C.

"Almost-standard-layout" C++ types have these requirements:

1. No virtual functions or virtual base classes i.e.
[`std::is_polymorphic_v<T>`](http://en.cppreference.com/w/cpp/types/is_polymorphic)
must be false. This is because the vptrs offset the proper front of the data layout
in an unknowable way to C.
2. Non-static data members of reference type appear to C as pointers. You
must never supply from C to C++ a non-null pointer which is seen as a reference in C++.
3. C++ inheritance is seen in C data layout as if the most derived class has nested
variables of the inherited types at the top, in order of inheritance.
4. Types with non-trivial destructors work fine so long as at least move construction
and assignment is the same as
copying bits like `memcpy()`. You just need to make sure instances of the type return
to C++, and don't get orphaned in C. This was referred to in previous pages in this
section as "move relocating" types.

Experimental Outcome's support for being used from C does not meet the current strict
requirements, and thus relies on the (very common) implementation defined behaviour just
described (it is hoped that future C++ standards can relax the requirements to those
just described).

Specifically, proposed `status_code` is an almost-standard-layout type,
and thus while it can't be returned from `extern "C"` functions as the compiler
will complain, it is perfectly safe to return from C++ functions to C code on the
three major compilers, as it is an "almost-standard-layout" C++ type if `T` is
an "almost-standard-layout" C++ type.
