// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/tuple.hpp>

#include <support/tracked.hpp>

#include <utility>
#include <memory>
namespace hana = boost::hana;


int main() {
    {
        using T = hana::tuple<std::unique_ptr<int>>;
        T t(std::unique_ptr<int>(new int(3)));
        std::unique_ptr<int> p = hana::at_c<0>(std::move(t));
        BOOST_HANA_RUNTIME_CHECK(*p == 3);
    }
    // make sure we don't double-move and do other weird stuff
    {
        hana::tuple<Tracked, Tracked, Tracked> xs{
            Tracked{1}, Tracked{2}, Tracked{3}
        };

        Tracked a = hana::at_c<0>(std::move(xs)); (void)a;
        Tracked b = hana::at_c<1>(std::move(xs)); (void)b;
        Tracked c = hana::at_c<2>(std::move(xs)); (void)c;
    }
    // test with nested closures
    {
        using Inner = hana::tuple<Tracked, Tracked>;
        hana::tuple<Inner> xs{Inner{Tracked{1}, Tracked{2}}};

        Tracked a = hana::at_c<0>(hana::at_c<0>(std::move(xs))); (void)a;
        Tracked b = hana::at_c<1>(hana::at_c<0>(std::move(xs))); (void)b;
    }
}
