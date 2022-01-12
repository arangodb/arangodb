+++
title = "Major differences"
weight = 20
+++

The major design differences between `<system_error>` and proposed `<system_error2>` are
as follows:

1. `experimental::status_code<DomainType>` can represent warnings
and form-of-success codes as well as failure codes. `experimental::errored_status_code<DomainType>`
is more similar to `std::error_code`, in that it can only represent failures
(this is enforced by C++ 20 contract or runtime assertion check).

2. The code's domain implementation defines the payload type to be transported around by
`experimental::status_code<DomainType>`, rather than it being
hardcoded to `int` as in `std::error_code`. The payload type can be anything
you like, including non-trivially-copyable, move-only, complex etc types.

    This facility is extremely useful. Extra failure metadata such as stack
backtraces can be returned, for example. You can absolutely vary the payload
depending on whether `NDEBUG` or `_DEBUG` is defined, too.

3. If your domain defines a payload type which is trivially copyable or
move relocating[^1], it gains an implicit convertibility to a move-only
`experimental::status_code<erased<T>>` where `T` is another
trivially copyable or move relocating type. This permits global headers
to use a single, common, type erased, status code type which is highly
desirable for code bases of any complexity. However, unlike `std::error_code`,
which fulfils the exact same role in `<system_error>` based code, the type
erased payload can be bigger than the hardcoded `int` in `std::error_code`.

    This facility is also extremely useful, as extra failure metadata can be
type erased, transported through code with no knowledge of such things,
and the original type with failure metadata resurrected at the handling point.
Indeed P1095 proposed `std::error` is a type alias to
`experimental::status_code<erased<intptr_t>>`, and it can transport erased
`std::exception_ptr` instances, POSIX error codes, and much more besides.

4. Equality comparisons between status code's with non-identical domains are
always <b><em>semantic</em></b> i.e. are they semantically equivalent, rather than exactly
equal? This mirrors when you compare `std::error_code` to a `std::error_condition`,
but as there is no equivalent for the latter in `<system_error2>`, this means
that when you see the code:

    ```c++
    if(code1 == code2) ...
    ```
    
    ... you can be highly confident that this is an inexact, semantic, match operation.
The same code under `<system_error>` is highly ambiguous as to whether exact
or inexact comparison is being performed (after all, all there is is "`code1 == code2`",
so it depends on the types of `code1` and `code2` which usually is not obvious).

    The ambiguity, and high cognitive load during auditing `<system_error>` code for correctness, has
led to many surprising and unexpected failure handling bugs during the past
decade in production C++.

5. `<system_error2>`, being a new design, has all-constexpr construction and
destruction which avoids the need for static global variables, as `<system_error>`
has. Each of those static global variables requires an atomic fence just in
case it has not been initialised, thus every retrieval of an error category bloats
code and inhibits optimisation, plus makes the use of custom error code categories
in header-only libraries unreliable. Boost.System has replicated the all-constexpr
construction and destruction from `<system_error2>`, and thus now has similar
characteristics in this regard.

6. Finally, this is a small but important difference. Under `<system_error>`,
this extremely common use case is ambiguous:

    ```c++
    if(ec) ...
    ```
    
    Does this code mean "if there was an error?", or "if the error code is set?",
or "is the error code non-zero?". The correct answer according to the standard is the last choice, but
a quick survey of open source `<system_error>` based code on github quickly
demonstrates there is widespread confusion regarding correct usage.

    `<system_error2>` solves this by removing the boolean test entirely. One
now writes `if(sc.success()) ...`, `if(sc.failure()) ...`, `if(sc.empty()) ...`
and so on, thus eliminating ambiguity.


[^1]: [Move relocating is not in the standard, it is currently within WG21 Evolution Working Group Incubator](http://wg21.link/P1029). It is defined to be a type whose move constructor `memcpy()`'s the bits from source to destination, followed by `memcpy()` of the bits of a default constructed instance to source, and with a programmer-given guarantee that the destructor, when called on a default constructed instance, has no observable side effects. A surprising number of standard library types can meet this definition of move relocating, including `std::vector<T>`, `std::shared_ptr<T>`, and `std::exception_ptr`.