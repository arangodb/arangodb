`cxx_function` library
======================

A prototype for new `std::function` features, compatible with C++11.

*by David Krauss (potatoswatter)*
<!-- language: lang-cxx -->

Highlights
==========

- Conforming to the `std::function` specification in C++11 and C++14
 - "Plus" `noexcept` guarantees for `move` and `swap`

- Multi-signature, overloaded function objects:

        function< ret1( arg1 ), void(), void() const >

 - Including reference qualification
 - Deprecation warning when a call operator casts away `const`

- Non-copyable target objects:

        unique_function< void() > f = [uptr = std::make_unique< resource_t >()] {…};

 - Including in-place construction

- Full support of allocators:

        function_container< my_pool<char>, void() > = other_pooled_function;

 - Custom `construct()` and `destroy()`
 - `std::scoped_allocator_adaptor` reaches inside target objects
 - Allocator propagation by assignment, swap, and copying (needs testing)
 - Fancy pointers (not yet tested)
 - Queries `allocator_traits` interface exclusively; does not access allocators directly

- Written in C++11; does not require migrating to a new language edition

- Efficiency
 - Economizes space better than libc++ (Clang)
 - Economizes branch instructions better than libstdc++ (GCC)
 - Performs well [compared](https://github.com/jamboree/CxxFunctionBenchmark) to similar third-party libraries.
 - Avoids indirect branches for trivial target object copy construction, move construction, and destruction
   (common case for lambdas)


How to use
==========

For `std::function` documentation, see [cppreference.com](http://en.cppreference.com/w/cpp/utility/functional/function)
or a [recent C++ draft standard](http://open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4527.pdf). Only new features are
described here.

This library is tested against Clang 3.6 (with libc++ and libstdc++) and GCC 5.1 (with libstdc++) on OS X.
There is a separate [branch](tree/msvc_port) tested against VS2015. Please [report](#bugs) bugs.

## Overloaded functions

Each signature supplied to a template will be used to declare one `operator()` overload.
Ordinary overload resolution is used to select between them. Each overload forwards its arguments along to the target.
There is no function template or perfect forwarding involved in dispatching, so you can pass braced initializer lists.

Duplicate signatures are currently ignored. Different permutations of an overload set produce different types, but the distinction
between them is not supported. Hopefully there ultimately may be a 1:1 mapping between overload sets and `function` types.

C++11 specifies that `std::function::operator()` is `const`-qualified, but implementations have always called the
target object without `const` qualification. This is supported but deprecated by this library. Any signature without
any function qualifiers (that is, supported by C++11) is shadowed by a `const`-qualified version. Calling this function
will produce a deprecation warning. You can replace it by declaring the `const`-qualified signature in the list.

There are several general solutions to a deprecation warning:

1.  Pass the `function` by value or forwarding so it is not observed to be `const`.

    This has always been the canonical usage of `std::function` (and all Callable objects). This fix can be applied per call
    site, usually without affecting any external interface.

2.  Add a `const` qualifier to the signature. This explicitly solves the problem through greater exposition and tighter
    constraints. It requires that the target object be callable as `const`.
    
    This is usually a good idea in any case, since in practice most functions are not stateful. Ideally,
    `function< void() const >` should represent a function that *does the same thing on each call*. If you have
    `function< void() > const` instead, that means that calling may change some state, but you don't have permission to do
    so. (This is the crux of the issue.)
    
    If the target needs to change, but only in a way that doesn't affect its observable behavior, consider using `mutable` instead.
    Note that lambdas allow `mutable`, but the keyword is somewhat abused: the members of the lambda are not `mutable`.
    It only makes the call operator non-`const`. You will need to explicitly declare a class with a `mutable` member.

3.  Consistently remove `const` from the reference which gets called. Non-`const` references are the best way to share
    mutable access to values. More `const` is not necessarily better. Again, this reflects greater exposition and tighter
    constraints.
    
    If you *must* use a `const_cast`, do so within the target call handler, not on the `function` which gets called. The
    validity of `const_cast` depends on whether or not the affected object was created as `const`, and target objects never are.

If you declare a signature with no qualifiers and one with e.g. `const &` qualifiers, the automatic `const` overload will
cause an overload resolution conflict. Declare a pair of `&` and `&&` qualifiers instead. (Any such design would be very
suspicious, though.)

A target object is checked against all the signatures when it is assigned to a wrapper. Like permutations of the same
overload set, you can assign one `function` to another specialization which is different but appropriately callable.
The result will be two `function` objects, one wrapped inside the other. This is Bad Design and it should probably be
deprecated.


## Non-copyable functions

The `unique_function` template works just like `function`, but it can handle any target object and it cannot be copied.
To construct the target object in place (to avoid moving it), pass a dispatch tag like this (in C++11):

    unique_function< void() > fn( in_place_t< target_type >{}, constructor_arg1, arg2 );

or this (since C++14):

    unique_function< void() > fn( in_place< target_type >, constructor_arg1, arg2 );

This also works with all the other templates. Likewise, there are new functions for assignent after construction:

    fn.emplace_assign< target_type >( constructor_arg1, arg2 );
    fn.allocate_assign< target_type >( allocator, constructor_arg1, arg2 );

You can assign any of the other templates to a `unique_function`, but not vice versa. There is no risk of double wrapping, as
long as the signature lists match exactly. Whereas other implementations tend to throw an error deep from deep inside their
templates for a non-copyable target, this library checks copyability for the non-`unique` types before anything else happens.

Note that copyability is different from an rvalue-qualified (`&&`) signature. For example:

- `function< void() && >` ideally represents a function that can be copied, and called once per copy.
- `unique_function< void() >` represents a function with abilities that can't be replicated in a copy (hence "unique"),
  but can still be called many times.
- `unique_function< void() && >` represents a one-shot function that can only ever be called once.

To call an object with an rvalue-qualified signature, use `std::move( my_func ) ( args )`. This usage also works on ordinary,
unqualified signatures and it is backward-compatible with C++11 `std::function`.

### Movable and non-movable targets

A target which is not movable must be initialized using in-place initialization.

A target with a move constructor that is not declared `noexcept` (or `throw()`, which is deprecated) will be allocated on the
heap; it cannot be stored within the wrapper. Once a target is placed on the heap, it will never be moved at all, except to
an allocator representing a different memory pool. (See below.) It's better to remember the `noexcept`. Also, use `= default`
when possible, as it automatically computes and sets `noexcept` for you.


## Enhanced safety

A function object returning `const &` or `&&` should avoid returning a reference to a temporary. This library forbids target
objects whose return type would force `function` to return a dangling reference. The standard library currently allows them.

    function< int const &() > fn = []{ return 5; }; // Binding 5 to int const& would produce a dangling return value.

This protection is not perfect. It may be defeated by implicit conversion between class types. This slips through the cracks:

    function< std::pair< long, long > const &() > fn = []{ return std::make_pair( 1, 2 ); };


## Allocators

This library implements a new allocator model, conforming (almost, see "Bugs" below) to the C++11 specification. (How can it be both
new and old? Because the normative `std::function` allocator model has been underspecified, there are several equally-conforming
models.) As for `function` alone, this library behaviorally matches libc++ and libstdc++, but additionally supporting the allocator
methods `construct` and `destroy`.

`function` and `unique_function` use an allocator to handle target objects that cannot fit into the wrapper storage (equal to
the size of three pointers). They behave as clients of the allocator, but not as containers. The behavior of `std::shared_ptr`
via `std::allocate_shared` is closely analogous: the allocator is used when the object is created and destroyed, but it is
invisible in the meantime. The only difference is that `function` permits additional operations like copying.

`function_container` and `unique_function_container` behave more like allocator-aware containers. They encapsulate a permanent
allocator instance, of a type given by the initial template argument. The target object of these wrappers always uses either the
given allocator, or only the storage within the wrapper. When a pre-existing target is assigned from a corresponding wrapper
specialization, it's adopted by following these steps:

1.  If the allocator specifies `propagate_on_container_move_assignment` or `propagate_on_container_copy_assignment`
    (whichever is applicable) as `std::true_type`, and the source is also a container, then the allocator is assigned.

2.  If the target is stored within the wrapper, allocators aren't an issue. It's copied or moved directly, using `memcpy` if
    possible.

3.  Otherwise, the type of the new target's allocator is compared to the type of the container allocator. Both are rebound to
    `Allocator< char >` and the resulting types are compared using `typeid`.

4.  **If the types don't match, an exception is thrown!** Do not let this happen: only assign to `function_container` when
    you know the memory scheme originating the assigned value. The type of the exception is `allocator_mismatch_error`.

5.  Otherwise, the allocators are compared using the `==` operator. Again, both instances are rebound to a `value_type` of 
    `char`. If they compare equal, then the assignment proceeds as if the container were just a non-container `function`.
    
    If they are unequal, or if it's a copy-assignment, the target is reallocated by the container's allocator. The
    container's allocator is then updated to reflect any change. (The new value is still equal to the old value, though.)

6.  In any case, if the operation is a move, the source wrapper is set to `nullptr`. This guarantees deallocation from the
    source memory pool when the destination pool is different.

Note that initializing a `function_container` from a `function` will not adopt the allocator. It will use a default-initialized
allocator. Ideally, the compiler will complain that the allocator cannot be default-initialized. These constructor overloads
should probably be deprecated, because it looks like the allocator is getting dynamically copy/move-initialized.

Since the allocator of `function_container` is fixed, it does not implement the `assign` or `allocate_assign` functions.


### Propagation

Container allocators are "sticky:" the allocator designated at initialization is used throughout the container's lifetime.

Non-container `function` allocators are "stealthy:" the allocator is only used to manage the target object, and any copies
of it. It cannot be observed by `get_allocator`. (It really does exist though, and an accessor could easily be implemented.)

There exist allocator properties `propagate_on_container_move_assignment` and `propagate_on_container_copy_assignment`.
These affect the semantics of assignment between containers, but do not enable a container to observe the allocator of a
non-container. They reduce stickiness, but not stealthiness.

POCMA and POCCA have a similar effect on `function_container` as on any other container such as `std::vector`. In particular,
POCMA is sufficient for container move assignment to be `noexcept`. However, `is_always_equal` is also sufficient for that,
and using these traits is a serious anti-pattern: Allocators are supposed to be orthogonal to containers. `is_always_equal`
furthermore eliminates the requirement that the target be move-constructible, which POCMA does not do. It is the better
alternative. (However, `is_always_equal` support is only getting added to libc++ and libstdc++ as I write this.)


### Usage

Memory management gets harder as it gets stricter. Here are some ground rules for this library:

1.  Use best practices for container allocators. Use `scoped_allocator_adaptor`.

2.  Do not assign something to a `function_container` unless it was created using a compatible `function_container`,
    or you're *sure* it was created using `allocator_arg_t` with a `function`.

3.  Use `function_container` only for persistent storage; don't pass it around everywhere. It's not suitable as a value type.
    Use `function` for that. Do not try to hack the allocator model using `propagate_on_container_*_assignment`.

4.  If you're afraid about copies running against resource constraints, use `unique_*`. Note that when you give a `function`
    to someone, they may (in theory) make unlimited copies of it. On the other hand, resource exhaustion is something that
    allocators manage, so you should be prepared for this case and not "afraid" of it :) .

5.  The memory resource backing the allocators must persist as long as any functions may be alive and using the storage.
    Always assert that memory resources (pools) are empty before freeing them, lest references go dangling. Do not pass
    custom-allocated functions to a library that will retain them indefinitely, unless you're willing to take a risk by either
    (a) destroying the pool at the last possible moment before program termination or (b) not destroying the pool at all.


Bugs
====

Please report issues [on GitHub](https://github.com/potswa/cxx_function/issues). Here are some that are already known:

- `function` and `unique_function` constructors taking an allocator but no target should be deprecated, and should accept
  any allocator type. Instead, they require a `std::allocator` specialization and give no warning.
  
  Note that this constructor is not implemented at all by libstdc++ (GCC), and it doesn't do anything in libc++ (Clang),
  so unless you already have a more exotic library, you should not be using it. The most likely result of this bug is simply
  that usages needing migration to `function_container` get flagged with overload resolution failures.

- The allocator and the pointer it generates (which may be a "fancy pointer," or a type other than `T*`) must fit into the
  wrapper storage space, equal to three native pointers. There are subtle issues here requiring further research, but
  `std::allocate_shared` already practically solves this problem so I will probably end up doing the same thing.

- Double wrapping allows a function to be spread across multiple memory pools. There are many ways to accidentally forget
  to reallocate some owned resources, but the library could do a better job of catching mistakes.
  
  The same goes for all cases of double wrapping, really. None of them are semantically incorrect or involve privilege
  violations (such as accidentally allocating with `std::allocator`), but it's an easy mistake which wastes resources.


I'm not adding these bugs to the database myself. Feel free to do so. It helps to mention a motivating case. All are fixable.
