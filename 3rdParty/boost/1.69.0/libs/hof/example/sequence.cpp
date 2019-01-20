/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    sequence.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
/*=============================================================================
    Copyright (c) 2016 Paul Fultz II
    print.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include "example.h"
#include <tuple>

using namespace boost::hof;

// Transform each element of a tuple by calling f
BOOST_HOF_STATIC_LAMBDA_FUNCTION(tuple_transform) = [](auto&& sequence, auto f)
{
    return unpack(proj(f, construct<std::tuple>()))(std::forward<decltype(sequence)>(sequence));
};
// Call f on each element of tuple
BOOST_HOF_STATIC_LAMBDA_FUNCTION(tuple_for_each) = [](auto&& sequence, auto f)
{
    return unpack(proj(f))(std::forward<decltype(sequence)>(sequence));
};
// Fold over tuple using a f as the binary operator
BOOST_HOF_STATIC_LAMBDA_FUNCTION(tuple_fold) = [](auto&& sequence, auto f)
{
    return unpack(fold(f))(std::forward<decltype(sequence)>(sequence));
};
// Concat multiple tuples
BOOST_HOF_STATIC_FUNCTION(tuple_cat) = unpack(construct<std::tuple>());
// Join a tuple of tuples into just a tuple
BOOST_HOF_STATIC_FUNCTION(tuple_join) = unpack(tuple_cat);
// Filter elements in a tuple using a predicate
BOOST_HOF_STATIC_LAMBDA_FUNCTION(tuple_filter) = [](auto&& sequence, auto predicate)
{
    return compose(tuple_join, tuple_transform)(
        std::forward<decltype(sequence)>(sequence),
        [&](auto&& x)
        {
            return first_of(
                if_(predicate(std::forward<decltype(x)>(x)))(pack),
                always(pack())
            )(std::forward<decltype(x)>(x));
        }
    );
};
// Zip two tuples together
BOOST_HOF_STATIC_LAMBDA_FUNCTION(tuple_zip_with) = [](auto&& sequence1, auto&& sequence2, auto f)
{
    auto&& functions = tuple_transform(
        std::forward<decltype(sequence1)>(sequence1),
        [&](auto&& x)
        {
            return [&](auto&& y)
            {
                return f(std::forward<decltype(x)>(x), std::forward<decltype(y)>(y));
            };
        }
    );
    auto combined = unpack(capture(construct<std::tuple>())(combine))(functions);
    return unpack(combined)(std::forward<decltype(sequence2)>(sequence2));
};
// Dot product of a tuple
BOOST_HOF_STATIC_LAMBDA_FUNCTION(tuple_dot) = [](auto&& a, auto&& b)
{
    auto product = tuple_zip_with(a, b, [](auto x, auto y) { return x*y; });
    return tuple_fold(product, [](auto x, auto y) { return x+y; });
};

void run_each()
{
    auto t = std::make_tuple(1, 2);
    tuple_for_each(t, [](int i) { std::cout << i << std::endl; });
}

void run_transform()
{
    auto t = std::make_tuple(1, 2);
    auto r = tuple_transform(t, [](int i) { return i*i; });
    assert(r == std::make_tuple(1, 4));
    (void)r;
}

void run_filter()
{
    auto t = std::make_tuple(1, 2, 'x', 3);
    auto r = tuple_filter(t, [](auto x) { return std::is_same<int, decltype(x)>(); });
    assert(r == std::make_tuple(1, 2, 3));
    (void)r;
}

void run_zip()
{
    auto t1 = std::make_tuple(1, 2);
    auto t2 = std::make_tuple(3, 4);
    auto p = tuple_zip_with(t1, t2, [](auto x, auto y) { return x*y; });
    int r = tuple_fold(p, [](auto x, auto y) { return x+y; });
    assert(r == (1*3 + 4*2));
    (void)r;
}

void run_dot()
{
    auto t1 = std::make_tuple(1, 2);
    auto t2 = std::make_tuple(3, 4);
    int r = tuple_dot(t1, t2);
    assert(r == (1*3 + 4*2));
    (void)r;
}

int main() 
{
    run_transform();
    run_filter();
    run_zip();
}

