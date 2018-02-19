// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/and.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/comparing.hpp>
#include <boost/hana/div.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/eval_if.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/greater_equal.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/less_equal.hpp>
#include <boost/hana/max.hpp>
#include <boost/hana/min.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/mod.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/negate.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/one.hpp>
#include <boost/hana/or.hpp>
#include <boost/hana/ordering.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/power.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/while.hpp>
#include <boost/hana/zero.hpp>

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
namespace hana = boost::hana;


struct invalid {
    template <typename T>
    operator T const() { std::abort(); }
};

int main() {
    //////////////////////////////////////////////////////////////////////////
    // Comparable
    //////////////////////////////////////////////////////////////////////////
    {
        hana::test::_injection<0> f{};
        auto x = numeric(1);
        auto y = numeric(2);

        // equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(x, x));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(x, y)));
        }

        // not_equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_equal(x, y));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::not_equal(x, x)));
        }

        // comparing
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::comparing(f)(x, x),
                hana::equal(f(x), f(x))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::comparing(f)(x, y),
                hana::equal(f(x), f(y))
            ));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Orderable
    //////////////////////////////////////////////////////////////////////////
    {
        auto ord = numeric;

        // _injection is also monotonic
        hana::test::_injection<0> f{};

        // less
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::less(ord(0), ord(1)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord(0), ord(0))));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord(1), ord(0))));
        }

        // less_equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::less_equal(ord(0), ord(1)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::less_equal(ord(0), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less_equal(ord(1), ord(0))));
        }

        // greater_equal
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::greater_equal(ord(1), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::greater_equal(ord(0), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::greater_equal(ord(0), ord(1))));
        }

        // greater
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::greater(ord(1), ord(0)));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::greater(ord(0), ord(0))));
            BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::greater(ord(0), ord(1))));
        }

        // max
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::max(ord(0), ord(0)), ord(0)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::max(ord(1), ord(0)), ord(1)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::max(ord(0), ord(1)), ord(1)
            ));
        }

        // min
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::min(ord(0), ord(0)),
                ord(0)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::min(ord(1), ord(0)),
                ord(0)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::min(ord(0), ord(1)),
                ord(0)
            ));
        }

        // ordering
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::ordering(f)(ord(1), ord(0)),
                hana::less(f(ord(1)), f(ord(0)))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::ordering(f)(ord(0), ord(1)),
                hana::less(f(ord(0)), f(ord(1)))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::ordering(f)(ord(0), ord(0)),
                hana::less(f(ord(0)), f(ord(0)))
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
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::zero<Numeric>(), numeric(0)
            ));
        }

        // plus
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::plus(numeric(x), numeric(y)),
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
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::minus(numeric(x), numeric(y)),
                numeric(x - y)
            ));
        }

        // negate
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::negate(numeric(x)),
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
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::one<Numeric>(),
                numeric(1)
            ));
        }

        // mult
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::mult(numeric(x), numeric(y)),
                numeric(x * y)
            ));
        }

        // power
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::power(numeric(x), hana::zero<CNumeric<int>>()),
                hana::one<Numeric>()
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::power(numeric(x), hana::one<CNumeric<int>>()),
                numeric(x)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::power(numeric(x), cnumeric<int, 2>),
                hana::mult(numeric(x), numeric(x))
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::power(numeric(x), cnumeric<int, 3>),
                hana::mult(hana::mult(numeric(x), numeric(x)), numeric(x))
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
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::div(numeric(x), numeric(y)),
                numeric(x / y)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::div(numeric(x), numeric(z)),
                 numeric(x/ z)
            ));
        }

        // mod
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::mod(numeric(x), numeric(y)),
                numeric(x % y)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::mod(numeric(x), numeric(z)),
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
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::not_(logical(true)),
                logical(false)
            ));
        }

        // and_
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(false)),
                logical(false)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(true), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(true), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(false), invalid{}),
                logical(false)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(true), logical(true), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(true), logical(true), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(true), logical(false), invalid{}),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::and_(logical(false), invalid{}, invalid{}),
                logical(false)
            ));
        }

        // or_
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(false)),
                logical(false)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(false), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(false), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(true), invalid{}),
                logical(true)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(false), logical(false), logical(false)),
                logical(false)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(false), logical(false), logical(true)),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(false), logical(true), invalid{}),
                logical(true)
            ));
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::or_(logical(true), invalid{}, invalid{}),
                logical(true)
            ));
        }

        // if_
        {
            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::if_(logical(true), comparable(0), comparable(1)),
                comparable(0)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::if_(logical(false), comparable(0), comparable(1)),
                comparable(1)
            ));
        }

        // eval_if
        {
            auto t = [=](auto) { return comparable(0); };
            auto e = [=](auto) { return comparable(1); };

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::eval_if(logical(true), t, e),
                comparable(0)
            ));

            BOOST_HANA_CONSTEXPR_CHECK(hana::equal(
                hana::eval_if(logical(false), t, e),
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

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(smaller_than(0u), std::vector<int>{}, f),
                std::vector<int>{}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(smaller_than(1u), std::vector<int>{}, f),
                std::vector<int>{0}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(smaller_than(2u), std::vector<int>{}, f),
                std::vector<int>{0, 1}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(smaller_than(3u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(smaller_than(4u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2, 3}
            ));

            // Make sure it can be called with an lvalue state:
            std::vector<int> v{};
            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(smaller_than(4u), v, f),
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

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(less_than(0u), std::vector<int>{}, f),
                std::vector<int>{}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(less_than(1u), std::vector<int>{}, f),
                std::vector<int>{0}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(less_than(2u), std::vector<int>{}, f),
                std::vector<int>{0, 1}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(less_than(3u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2}
            ));

            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(less_than(4u), std::vector<int>{}, f),
                std::vector<int>{0, 1, 2, 3}
            ));

            // Make sure it can be called with an lvalue state:
            std::vector<int> v{};
            BOOST_HANA_RUNTIME_CHECK(hana::equal(
                hana::while_(less_than(4u), v, f),
                std::vector<int>{0, 1, 2, 3}
            ));
        }
    }
}
