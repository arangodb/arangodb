//-----------------------------------------------------------------------------
// boost-libs variant/test/variant_plymorphic_get_test.cpp source file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2003 Eric Friedman
// Copyright (c) 2013-2014 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "boost/variant/variant.hpp"
#include "boost/variant/apply_visitor.hpp"
#include "boost/variant/static_visitor.hpp"
#include "boost/variant/polymorphic_get.hpp"
#include "boost/test/minimal.hpp"

struct base {int trash;};
struct derived1 : base{};
struct derived2 : base{};

struct vbase { short trash; virtual ~vbase(){} virtual int foo() const { return 0; }  };
struct vderived1 : virtual vbase{ virtual int foo() const { return 1; } };
struct vderived2 : virtual vbase{ virtual int foo() const { return 3; } };
struct vderived3 : vderived1, vderived2 { virtual int foo() const { return 3; } };

template <class T, class Variant>
inline void check_throws(Variant& v) {
    try {
        boost::polymorphic_get<T>(v);
        BOOST_CHECK(false);
    } catch (const boost::bad_polymorphic_get& e) {
        BOOST_CHECK(!!e.what());
        BOOST_CHECK(std::string(e.what()) != boost::bad_get().what());
    }
}

int test_main(int , char* [])
{
    typedef boost::variant<int, base, derived1, derived2> var_t;

    var_t var1;
    BOOST_CHECK(!boost::polymorphic_get<base>(&var1));
    check_throws<base>(var1);
    BOOST_CHECK(!boost::polymorphic_get<const base>(&var1));
    check_throws<base, const var_t>(var1);

    var1 = derived1();
    BOOST_CHECK(boost::polymorphic_get<base>(&var1));
    BOOST_CHECK(boost::polymorphic_get<const base>(&var1));

    derived2 d;
    d.trash = 777;
    var_t var2 = d;
    BOOST_CHECK(boost::polymorphic_get<base>(var2).trash == 777);
    BOOST_CHECK(boost::polymorphic_get<const base>(var2).trash == 777);

    var2 = 777;
    BOOST_CHECK(!boost::polymorphic_get<base>(&var2));
    check_throws<base>(var2);
    BOOST_CHECK(!boost::polymorphic_get<const base>(&var2));
    check_throws<base, const var_t>(var2);
    BOOST_CHECK(boost::polymorphic_get<int>(var2) == 777);
    BOOST_CHECK(boost::polymorphic_get<const int>(var2) == 777);

    typedef boost::variant<int, vbase, vderived1, vderived2, vderived3> vvar_t;

    vvar_t v = vderived3();
    boost::polymorphic_get<vderived3>(v).trash = 777;
    const vvar_t& cv = v;
    BOOST_CHECK(boost::polymorphic_get<vbase>(cv).trash == 777);
    BOOST_CHECK(boost::polymorphic_get<const vbase>(cv).trash == 777);

    BOOST_CHECK(boost::polymorphic_get<vbase>(cv).foo() == 3);
    BOOST_CHECK(boost::polymorphic_get<vbase>(v).foo() == 3);
    BOOST_CHECK(boost::polymorphic_get<const vbase>(cv).foo() == 3);
    BOOST_CHECK(boost::polymorphic_get<const vbase>(v).foo() == 3);

    return boost::exit_success;
}
