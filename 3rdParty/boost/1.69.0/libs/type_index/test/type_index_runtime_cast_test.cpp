//
// Copyright Chris Glover, 2016.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// #include <boost/type_index/runtime_cast.hpp>
// #include <boost/type_index/runtime_reference_cast.hpp>

#include <boost/type_index/runtime_cast.hpp>
#include <boost/type_index/runtime_cast/boost_shared_ptr_cast.hpp>
#include <boost/smart_ptr/make_shared.hpp>

#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_NO_CXX11_SMART_PTR)
#  include <boost/type_index/runtime_cast/std_shared_ptr_cast.hpp>
#endif

// Classes include a member variable "name" with the
// name of the class hard coded so we can be sure that
// the pointer offsets are all working, since we're doing
// a cast from void* at some point.

#define IMPLEMENT_CLASS(type_name) \
        type_name() : name( #type_name ) {} \
        std::string name;

struct base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST(BOOST_TYPE_INDEX_NO_BASE_CLASS)
    IMPLEMENT_CLASS(base)
};

struct single_derived : base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base))
    IMPLEMENT_CLASS(single_derived)
};

struct base1 {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST(BOOST_TYPE_INDEX_NO_BASE_CLASS)
    IMPLEMENT_CLASS(base1)
};

struct base2 {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST(BOOST_TYPE_INDEX_NO_BASE_CLASS)
    IMPLEMENT_CLASS(base2)
};

struct multiple_derived : base1, base2 {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base1)(base2))
    IMPLEMENT_CLASS(multiple_derived)
};

struct baseV1 : virtual base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base))
    IMPLEMENT_CLASS(baseV1)
};

struct baseV2 : virtual base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base))
    IMPLEMENT_CLASS(baseV2)
};

struct multiple_virtual_derived : baseV1, baseV2 {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((baseV1)(baseV2))
    IMPLEMENT_CLASS(multiple_virtual_derived)
};

struct unrelated {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST(BOOST_TYPE_INDEX_NO_BASE_CLASS)
    IMPLEMENT_CLASS(unrelated)
};

struct unrelated_with_base : base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base))
    IMPLEMENT_CLASS(unrelated_with_base)
};

struct unrelatedV1 : virtual base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base))
    IMPLEMENT_CLASS(unrelatedV1)
};

struct level1_a : base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base))
    IMPLEMENT_CLASS(level1_a)
};

struct level1_b : base {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((base))
    IMPLEMENT_CLASS(level1_b)
};

struct level2 : level1_a, level1_b {
    BOOST_TYPE_INDEX_IMPLEMENT_RUNTIME_CAST((level1_a)(level1_b))
    IMPLEMENT_CLASS(level2)
};

struct reg_base {
    BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS(BOOST_TYPE_INDEX_NO_BASE_CLASS)
};

struct reg_derived : reg_base {
    BOOST_TYPE_INDEX_REGISTER_RUNTIME_CLASS((reg_base))
};

void no_base()
{
    using namespace boost::typeindex;
    base b;
    base* b2 = runtime_pointer_cast<base>(&b);
    BOOST_TEST_NE(b2, (base*)NULL);
    BOOST_TEST_EQ(b2->name, "base");

    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(&b), (unrelated*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<single_derived>(&b), (single_derived*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelatedV1>(&b), (unrelatedV1*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated_with_base>(&b), (unrelated_with_base*)NULL);
}

void single_base()
{
    using namespace boost::typeindex;
    single_derived d;
    base* b = &d;
    single_derived* d2 = runtime_pointer_cast<single_derived>(b);
    BOOST_TEST_NE(d2, (single_derived*)NULL);
    BOOST_TEST_EQ(d2->name, "single_derived");

    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(&d), (unrelated*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(b), (unrelated*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated_with_base>(b), (unrelated_with_base*)NULL);
}

void multiple_base()
{
    using namespace boost::typeindex;
    multiple_derived d;
    base1* b1 = &d;
    multiple_derived* d2 = runtime_pointer_cast<multiple_derived>(b1);
    BOOST_TEST_NE(d2, (multiple_derived*)NULL);
    BOOST_TEST_EQ(d2->name, "multiple_derived");

    base2* b2 = runtime_pointer_cast<base2>(b1);
    BOOST_TEST_NE(b2, (base2*)NULL);
    BOOST_TEST_EQ(b2->name, "base2");

    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(&d), (unrelated*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(b1), (unrelated*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated_with_base>(b1), (unrelated_with_base*)NULL);
}

void virtual_base()
{
    using namespace boost::typeindex;
    multiple_virtual_derived d;
    base* b = &d;
    multiple_virtual_derived* d2 = runtime_pointer_cast<multiple_virtual_derived>(b);
    baseV1* bv1 = runtime_pointer_cast<baseV1>(b);
    baseV2* bv2 = runtime_pointer_cast<baseV2>(b);

    BOOST_TEST_NE(d2, (multiple_virtual_derived*)NULL);
    BOOST_TEST_EQ(d2->name, "multiple_virtual_derived");

    BOOST_TEST_NE(bv1, (baseV1*)NULL);
    BOOST_TEST_EQ(bv1->name, "baseV1");

    BOOST_TEST_NE(bv2, (baseV2*)NULL);
    BOOST_TEST_EQ(bv2->name, "baseV2");

    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(b), (unrelated*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(&d), (unrelated*)NULL);
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated_with_base>(b), (unrelated_with_base*)NULL);
}

void pointer_interface()
{
    using namespace boost::typeindex;
    single_derived d;
    base* b = &d;
    single_derived* d2 = runtime_cast<single_derived*>(b);
    BOOST_TEST_NE(d2, (single_derived*)NULL);
    BOOST_TEST_EQ(d2->name, "single_derived");
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(b), (unrelated*)NULL);
}

void reference_interface()
{
    using namespace boost::typeindex;
    single_derived d;
    base& b = d;
    single_derived& d2 = runtime_cast<single_derived&>(b);
    BOOST_TEST_EQ(d2.name, "single_derived");

    try {
        unrelated& u = runtime_cast<unrelated&>(b);
        (void)u;
        BOOST_TEST(!"should throw bad_runtime_cast");
    }
    catch(boost::typeindex::bad_runtime_cast&) {
    }
    catch(...) {
        BOOST_TEST(!"should throw bad_runtime_cast");
    }
}

void const_pointer_interface()
{
    using namespace boost::typeindex;
    const single_derived d;
    base const* b = &d;
    single_derived const* d2 = runtime_cast<single_derived const*>(b);
    BOOST_TEST_NE(d2, (single_derived*)NULL);
    BOOST_TEST_EQ(d2->name, "single_derived");
    BOOST_TEST_EQ(runtime_pointer_cast<unrelated>(b), (unrelated*)NULL);
}

void const_reference_interface()
{
    using namespace boost::typeindex;
    const single_derived d;
    base const& b = d;
    single_derived const& d2 = runtime_cast<single_derived const&>(b);
    BOOST_TEST_EQ(d2.name, "single_derived");

    try {
        unrelated const& u = runtime_cast<unrelated const&>(b);
        (void)u;
        BOOST_TEST(!"should throw bad_runtime_cast");
    }
    catch(boost::typeindex::bad_runtime_cast&) {
    }
    catch(...) {
        BOOST_TEST(!"should throw bad_runtime_cast");
    }
}

void diamond_non_virtual()
{
    using namespace boost::typeindex;
    level2 inst;
    level1_a* l1a = &inst;
    base* b1 = l1a;
    level1_b* l1_b = runtime_cast<level1_b*>(b1);
    BOOST_TEST_NE(l1_b, (level1_b*)NULL);
    BOOST_TEST_EQ(l1_b->name, "level1_b");
}

void boost_shared_ptr()
{
    using namespace boost::typeindex;
    boost::shared_ptr<single_derived> d = boost::make_shared<single_derived>();
    boost::shared_ptr<base> b = d;
    boost::shared_ptr<single_derived> d2 = runtime_pointer_cast<single_derived>(b);
    BOOST_TEST_NE(d2, boost::shared_ptr<single_derived>());
    BOOST_TEST_EQ(d2->name, "single_derived");
}

void std_shared_ptr()
{
#if !defined(BOOST_NO_CXX11_SMART_PTR)
    using namespace boost::typeindex;
    std::shared_ptr<single_derived> d = std::make_shared<single_derived>();
    std::shared_ptr<base> b = d;
    std::shared_ptr<single_derived> d2 = runtime_pointer_cast<single_derived>(b);
    BOOST_TEST_NE(d2, std::shared_ptr<single_derived>());
    BOOST_TEST_EQ(d2->name, "single_derived");
#endif
}

void register_runtime_class()
{
    using namespace boost::typeindex;
    reg_derived rd;
    reg_base* rb = &rd;
    reg_derived* prd = runtime_pointer_cast<reg_derived>(rb);
    BOOST_TEST_NE(prd, (reg_derived*)NULL);
    BOOST_TEST_EQ(type_id_runtime(*prd), type_id<reg_derived>());
}

int main() {
    no_base();
    single_derived();
    multiple_base();
    virtual_base();
    pointer_interface();
    reference_interface();
    const_pointer_interface();
    const_reference_interface();
    diamond_non_virtual();
    boost_shared_ptr();
    std_shared_ptr();
    register_runtime_class();
    return boost::report_errors();
}
