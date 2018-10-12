<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

Partial function evaluation
===========================

Many of the adaptors(such as [partial](partial) or [pipable](pipable)) in the library supports optional partial evaluation of functions. For example, if we have the `sum` function adapted with the `partial` adaptor:

    auto sum = partial([](int x, int y)
    {
        return x+y;
    });

So if we write `sum(1, 2)` it will return 3, however, if we write `sum(1)` it will return a new function, which when called again, it will evaluate the function and return 3:

    int i = sum(1, 2); // Returns 3
    auto f = sum(1);
    int j = f(2); // returns 3

Of course due to limitations in C++, deciding whether evaluate the function or to partially evaluated it, is based on the callability of the function and not arity. So if we call `sum(1, 2, 3)`, it will return a function:

    auto f = sum(1, 2, 3);

However, this can get out of hande as the function `f` will never be evaluated. Plus, it would be nice to produce an error at the point of calling the function rather than a confusing error of trying to use a partial function. The [limit](limit) decorator lets us annotate the function with the max arity:

    auto sum = partial(limit_c<2>([](int x, int y)
    {
        return x+y;
    }));

So now if we call `sum(1, 2, 3)`, we will get a compiler error. So this improves the situation, but it is not without its limitations. For example if we were to define a triple sum using the [pipable](pipable) adaptor:

    auto sum = pipable(limit_c<3>([](int x, int y, int z)
    {
        return x+y+z;
    }));

So if we call `sum(1)`, there is no compiler error, not until we try to pipe in a value:

    auto f = sum(1); // No error here
    auto i = 2 | f; // Compile error

Of course, the goal may not be to use the pipable call, which could lead to some confusing errors. Currently, there is not a good solution to this.

