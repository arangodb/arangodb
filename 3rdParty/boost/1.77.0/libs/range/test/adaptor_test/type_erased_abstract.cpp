// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
#include <boost/range/adaptor/type_erased.hpp>
#include "type_erased_test.hpp"

#include <boost/test/unit_test.hpp>

#include <vector>

namespace boost_range_adaptor_type_erased_test
{
    namespace
    {

class dummy_interface
{
public:
    virtual ~dummy_interface() { }
    virtual void test() = 0;
protected:
    dummy_interface() { }
private:
    dummy_interface(const dummy_interface&);
    void operator=(const dummy_interface&);
};

class dummy_impl
    : public dummy_interface
{
public:
    dummy_impl() { }
    dummy_impl(const dummy_impl&) { }
    dummy_impl& operator=(const dummy_impl&) { return *this; }
    virtual void test() { }
};

typedef boost::any_range<
    dummy_interface,
    boost::random_access_traversal_tag,
    dummy_interface&,
    std::ptrdiff_t
> any_interface_range;

struct foo_dummy_interface_fn
{
    void operator()(dummy_interface& iface)
    {
        iface.test();
    }
};

void foo_test_dummy_interface_range(any_interface_range rng)
{
    std::for_each(boost::begin(rng), boost::end(rng),
                  foo_dummy_interface_fn());
}

void test_type_erased_abstract()
{
    std::vector<dummy_impl> v(10);

    any_interface_range r(v);

    foo_test_dummy_interface_range(r);

    foo_test_dummy_interface_range(any_interface_range(v));
}

    } // anonymous namespace
} // namespace boost_range_adaptor_type_erased_test

boost::unit_test::test_suite*
init_unit_test_suite(int, char*[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE("RangeTestSuite.adaptor.type_erased_abstract");

    test->add(
        BOOST_TEST_CASE(
            &boost_range_adaptor_type_erased_test::test_type_erased_abstract));

    return test;
}
