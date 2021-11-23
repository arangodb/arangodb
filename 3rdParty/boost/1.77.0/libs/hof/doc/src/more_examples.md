<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

More examples
=============

As Boost.HigherOrderFunctions is a collection of generic utilities
related to functions, there is many useful cases with the library, but a key
point of many of these utilities is that they can solve these problems with
much simpler constructs than whats traditionally been done with
metaprogramming. Lets take look at some of the use cases for using Boost.HigherOrderFunctions.

Initialization
--------------

The [`BOOST_HOF_STATIC_FUNCTION`](/include/boost/hof/function) will help initialize function objects at
global scope, and will ensure that it is initialized at compile-time and (on
platforms that support it) will have a unique address across translation
units, thereby reducing executable bloat and potential ODR violations.

In addition, [`BOOST_HOF_STATIC_LAMBDA_FUNCTION`](/include/boost/hof/lambda) allows initializing a lambda
in the same manner. This allows for simple and compact function definitions
when working with generic lambdas and function adaptors.

Of course, the library can still be used without requiring global function
objects for those who prefer to avoid them will still find the library useful.


Projections
-----------

Instead of writing the projection multiple times in algorithms:

    std::sort(std::begin(people), std::end(people),
              [](const Person& a, const Person& b) {
                return a.year_of_birth < b.year_of_birth;
              });

We can use the [`proj`](/include/boost/hof/by) adaptor to project `year_of_birth` on the comparison
operator:

    std::sort(std::begin(people), std::end(people),
            proj(&Person::year_of_birth, _ < _));

Ordering evaluation of arguments
--------------------------------

When we write `f(foo(), bar())`, the standard does not guarantee the order in
which the `foo()` and `bar()` arguments are evaluated. So with `apply_eval` we
can order them from left-to-right:

    apply_eval(f, [&]{ return foo(); }, [&]{ return bar(); });

Extension methods
-----------------

Chaining many functions together, like what is done for range based libraries,
can make things hard to read:

    auto r = transform(
        filter(
            numbers,
            [](int x) { return x > 2; }
        ),
        [](int x) { return x * x; }
    );

It would be nice to write this:

    auto r = numbers
        .filter([](int x) { return x > 2; })
        .transform([](int x) { return x * x; });

The proposal [N4165](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n4165.pdf) 
for Unified Call Syntax(UFCS) would have allowed a function call of `x.f(y)` to become
`f(x, y)`. However, this was rejected by the comittee. So instead pipable functions can be
used to achieve extension methods. So it can be written like this:

    auto r = numbers
        | filter([](int x) { return x > 2; })
        | transform([](int x) { return x * x; });

Now, if some users feel a little worried about overloading the _bitwise or_
operator, pipable functions can also be used with [`flow`](/include/boost/hof/flow) like this:

    auto r = flow(
        filter([](int x) { return x > 2; }),
        transform([](int x) { return x * x; })
    )(numbers);

No fancy or confusing operating overloading and everything is still quite
readable.

