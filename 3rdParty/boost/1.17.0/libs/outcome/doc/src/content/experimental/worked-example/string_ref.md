+++
title = "String refs"
weight = 40
+++

`<system_error2>` does not use `std::string` to return possibly statically
or dynamically allocated strings, and thus avoids dragging in a lot of the
standard library which impacts build times.

Instead status code domains have a [`string_ref`](https://ned14.github.io/status-code/doc_status_code_domain.html#standardese-system_error2__status_code_domain__string_ref),
which has a polymorphic implementation which may or may not [manage a dynamic
memory allocation using an atomic reference counter](https://ned14.github.io/status-code/doc_status_code_domain.html#standardese-system_error2__status_code_domain__atomic_refcounted_string_ref). Due to this polymorphism, you don't
need to worry which implementation is actually in use under the bonnet
when you pass around `string_ref` instances.

`string_ref` provides the same member functions as a `span<const char>`,
and so participates ordinarily in STL algorithms and containers. In
particular, if you need to string search or slice it, you can construct a
`string_view` on top easily.

{{% snippet "experimental_status_code.cpp" "string_ref" %}}

Now you understand what `string_ref` does, returning the name of the
status code domain is self describing. Note we use the non-managing
constructor of `string_ref`, as the string `"file i/o error domain"`
is statically stored. We cache the returned value locally in static
storage.
