// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/logical.hpp>
#include <laws/monoid.hpp>
#include <laws/orderable.hpp>
#include <support/cnumeric.hpp>
#include <support/numeric.hpp>

#include <cstdlib>
#include <vector>
using namespace boost::hana;


struct invalid {
    template <typename T>
    operator T const() { std::abort(); }
};

int main() {
    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    {
        test::_injection<0> f{};
        auto x = numeric(1);
        auto y = numeric(2);

        // equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(x, x));
            BOOST_HANA_CONSTEXPR_CHECK(not_(equal(x, y)));
        }

        // not_equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(not_equal(x, y));
            BOOST_HANA_CONSTEXPR_CHECK(not_(not_equal(x, x)));
        }

        // comparing
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                comparing(f)(x, x),
                equal(f(x), f(x))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                comparing(f)(x, y),
                equal(f(x), f(y))
            ));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Orderable
    //////////////////////////////////////////////////////////////////////////
    {
        auto ord = numeric;

        // test::_injection is also monotonic
        test::_injection<0> f{};

        // less
        {
            BOOST_HANA_CONSTEXPR_CHECK(less(ord(0), ord(1)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(less(ord(0), ord(0))));
            BOOST_HANA_CONSTEXPR_CHECK(not_(less(ord(1), ord(0))));
        }

        // less_equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(less_equal(ord(0), ord(1)));
            BOOST_HANA_CONSTEXPR_CHECK(less_equal(ord(0), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(less_equal(ord(1), ord(0))));
        }

        // greater_equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(greater_equal(ord(1), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(greater_equal(ord(0), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(greater_equal(ord(0), ord(1))));
        }

        // greater
        {
            BOOST_HANA_CONSTEXPR_CHECK(greater(ord(1), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(not_(greater(ord(0), ord(0))));
            BOOST_HANA_CONSTEXPR_CHECK(not_(greater(ord(0), ord(1))));
        }

        // max
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                max(ord(0), ord(0)), ord(0)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                max(ord(1), ord(0)), ord(1)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                max(ord(0), ord(1)), ord(1)
            ));
        }

        // min
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                min(ord(0), ord(0)),
                ord(0)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                min(ord(1), ord(0)),
                ord(0)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                min(ord(0), ord(1)),
                ord(0)
            ));
        }

        // ordering
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                ordering(f)(ord(1), ord(0)),
                less(f(ord(1)), f(ord(0)))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                ordering(f)(ord(0), ord(1)),
                less(f(ord(0)), f(ord(1)))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                ordering(f)(ord(0), ord(0)),
                less(f(ord(0)), f(ord(0)))
            ));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Monoid
    //////////////////////////////////////////////////////////////////////////
    {
        constexpr int x = 2, y = 3;

        // zero
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                zero<Numeric>(), numeric(0)
            ));
        }

        // plus
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                plus(numeric(x), numeric(y)),
                numeric(x + y)
            ));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Group
    //////////////////////////////////////////////////////////////////////////
    {
        constexpr int x = 2, y = 3;

        // minus
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                minus(numeric(x), numeric(y)),
                numeric(x - y)
            ));
        }

        // negate
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                negate(numeric(x)),
                numeric(-x)
            ));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Ring
    //////////////////////////////////////////////////////////////////////////
    {
        constexpr int x = 2, y = 3;

        // one
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                one<Numeric>(),
                numeric(1)
            ));
        }

        // mult
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                mult(numeric(x), numeric(y)),
                numeric(x * y)
            ));
        }

        // power
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                power(numeric(x), zero<CNumeric<int>>()),
                one<Numeric>()
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                power(numeric(x), one<CNumeric<int>>()),
                numeric(x)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                power(numeric(x), cnumeric<int, 2>),
                mult(numeric(x), numeric(x))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                power(numeric(x), cnumeric<int, 3>),
                mult(mult(numeric(x), numeric(x)), numeric(x))
            ));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // EuclideanRing
    //////////////////////////////////////////////////////////////////////////
    {
        constexpr int x = 6, y = 3, z = 4;

        // div
        {
            using boost::hana::div; // hide ::div
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                div(numeric(x), numeric(y)),
                numeric(x / y)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                div(numeric(x), numeric(z)),
                 numeric(x/ z)
            ));
        }

        // mod
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                mod(numeric(x), numeric(y)),
                numeric(x % y)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                mod(numeric(x), numeric(z)),
                numeric(x % z)
            ));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Logical
    //////////////////////////////////////////////////////////////////////////
    {
        auto logical = numeric;
        auto comparable = numeric;

        // not_
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                not_(logical(true)),
                logical(false)
            ));
        }

        // and_
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(false)),
                logical(false)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(true), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(true), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(false), invalid{}),
                logical(false)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(true), logical(true), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(true), logical(true), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(true), logical(false), invalid{}),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                and_(logical(false), invalid{}, invalid{}),
                logical(false)
            ));
        }

        // or_
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(false)),
                logical(false)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(false), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(false), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(true), invalid{}),
                logical(true)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(false), logical(false), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(false), logical(false), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(false), logical(true), invalid{}),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                or_(logical(true), invalid{}, invalid{}),
                logical(true)
            ));
        }

        // if_
        {
            BOOST_HANA_CONSTEXPR_CHECK(equal(
                if_(logical(true), comparable(0), comparable(1)),
                comparable(0)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                if_(logical(false), comparable(0), comparable(1)),
                comparable(1)
            ));
        }

        // eval_if
        {
            auto t = [=](auto) { return comparable(0); };
            auto e = [=](auto) { return comparable(1); };

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                eval_if(logical(true), t, e),
                comparable(0)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(equal(
                eval_if(logical(false), t, e),
                comparable(1)
            ));
        }

        // while_
        {
            auto smaller_than = [](auto n) {
                return [n](auto v) { return v.size() < n; };
            };
            auto f = [](auto v) {
                v.push_back(v.size());
                return v;
            };

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(smaller_than(0u), std::vector<int>{}, f),
                std::vector<int>{}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(smaller_than(1u), std::vector<int>{}, f),
                std::vector<int>{0}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(smaller_than(2u), std::vector<int>{}, f),
                std::vector<int>{0, 1}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(smaller_than(3u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(smaller_than(4u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2, 3}
            ));

            // Make sure it can be called with an lvalue state:
            std::vector<int> v{};
            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(smaller_than(4u), v, f),
                std::vector<int>{0, 1, 2, 3}
            ));
        }

        // while_
        {
            auto less_than = [](auto n) {
                return [n](auto v) { return v.size() < n; };
            };
            auto f = [](auto v) {
                v.push_back(v.size());
                return v;
            };

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(less_than(0u), std::vector<int>{}, f),
                std::vector<int>{}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(less_than(1u), std::vector<int>{}, f),
                std::vector<int>{0}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(less_than(2u), std::vector<int>{}, f),
                std::vector<int>{0, 1}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(less_than(3u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2}
            ));

            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(less_than(4u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2, 3}
            ));

            // Make sure it can be called with an lvalue state:
            std::vector<int> v{};
            BOOST_HANA_RUNTIME_CHECK(equal(
                while_(less_than(4u), v, f),
                std::vector<int>{0, 1, 2, 3}
            ));
        }
    }
}
