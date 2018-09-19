<!-- Copyright 2018 Paul Fultz II
     Distributed under the Boost Software License, Version 1.0.
     (http://www.boost.org/LICENSE_1_0.txt)
-->

FAQ
===

#### Q: Why is `const` required for the call operator on function objects?

Mutable function objects are not prohibited, they just need to be explicit by
using the adaptor [`mutable_`](/include/boost/hof/mutable). The main reason for this, is that it can lead to
many suprising behaviours. Many times function objects are copied by value
everywhere. For example,

```cpp
struct counter
{
    int i;
    counter() : i(0)
    {}

    template<class... Ts>
    int operator()(Ts&&...)
    {
        return i++;
    }
};


counter c{};
proj(mutable_(c))(1,1);
// Prints 0, not 2
std::cout << c.i << std::endl;
```

The example won't ever yield the expected value, because the function mutates
a copy of the objects. Instead, `std::ref` should be used:

```cpp
counter c{};
proj(std::ref(c))(1,1);
// Prints 2
std::cout << c.i << std::endl;
```

Which will print the expected value.

Another reason why `const` is required is because of supporting `constexpr` on
C++11 compilers. In C++11, `constexpr` implies `const`, so it would be
impossible to provide a non-const overload for functions that is `constexpr`.
Instead, `constexpr` would have to be made explicit. Considering the pitfalls
of mutable function objects, it would be better to make mutability explicit
rather than `constexpr`.

#### Q: Is the reinterpret cast in BOOST_HOF_STATIC_LAMBDA undefined behaviour?

Not really, since the objects are empty, there is no data access. There is a
static assert to guard against this restriction.

Now there could be an insane implementation where this doesn't work(perhaps
the lambdas are not empty for some strange reason), which the library would
have to apply a different technique to make it work. However, this is quite
unlikely considering that C++ will probably get constexpr lambdas and inline
variables in the future.

Alternatively, the factory pattern can be used instead of
[`BOOST_HOF_STATIC_LAMBDA_FUNCTION`](BOOST_HOF_STATIC_LAMBDA_FUNCTION), which doesn't require an reinterpret cast:

```cpp
struct sum_factory
{
    auto operator*() const
    {
        return [](auto x, auto y)
        {
            return x + y;
        };
    }
}

BOOST_HOF_STATIC_FUNCTION(sum) = boost::hof::indirect(sum_factory{});
```
