//
// Copyright 2012-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/type_index.hpp>

#include <boost/lexical_cast.hpp>

#include <boost/core/lightweight_test.hpp>

namespace my_namespace1 {
    class my_class{};
}


namespace my_namespace2 {
    class my_class{};
}


void names_matches_type_id()
{
    using namespace boost::typeindex;
    BOOST_TEST_EQ(type_id<int>().pretty_name(), "int");
    BOOST_TEST_EQ(type_id<double>().pretty_name(), "double");

    BOOST_TEST_EQ(type_id<int>().name(),    type_id<int>().name());
    BOOST_TEST_NE(type_id<int>().name(),    type_id<double>().name());
    BOOST_TEST_NE(type_id<double>().name(), type_id<int>().name());
    BOOST_TEST_EQ(type_id<double>().name(), type_id<double>().name());
}

void default_construction()
{
    using namespace boost::typeindex;
    type_index ti1, ti2;
    BOOST_TEST_EQ(ti1, ti2);
    BOOST_TEST_EQ(type_id<void>(), ti1);

    BOOST_TEST_EQ(type_id<void>().name(),   ti1.name());
    BOOST_TEST_NE(type_id<int>(),           ti1);
}


void copy_construction()
{
    using namespace boost::typeindex;
    type_index ti1, ti2 = type_id<int>();
    BOOST_TEST_NE(ti1, ti2);
    ti1 = ti2;    
    BOOST_TEST_EQ(ti2, ti1);

    const type_index ti3(ti1);
    BOOST_TEST_EQ(ti3, ti1);
}

void comparators_type_id()
{
    using namespace boost::typeindex;
    type_index t_int = type_id<int>();
    type_index t_double = type_id<double>();

    BOOST_TEST_EQ(t_int, t_int);
    BOOST_TEST_LE(t_int, t_int);
    BOOST_TEST_GE(t_int, t_int);
    BOOST_TEST_NE(t_int, t_double);

    BOOST_TEST_LE(t_double, t_double);
    BOOST_TEST_GE(t_double, t_double);
    BOOST_TEST_NE(t_double, t_int);

    BOOST_TEST(t_double < t_int || t_int < t_double);
    BOOST_TEST(t_double > t_int || t_int > t_double);
}

void hash_code_type_id()
{
    using namespace boost::typeindex;
    std::size_t t_int1 = type_id<int>().hash_code();
    std::size_t t_double1 = type_id<double>().hash_code();

    std::size_t t_int2 = type_id<int>().hash_code();
    std::size_t t_double2 = type_id<double>().hash_code();

    BOOST_TEST_EQ(t_int1, t_int2);
    BOOST_TEST_NE(t_int1, t_double2);
    BOOST_TEST_LE(t_double1, t_double2);
}



template <class T1, class T2>
static void test_with_modofiers() {
    using namespace boost::typeindex;

    type_index t1 = type_id_with_cvr<T1>();
    type_index t2 = type_id_with_cvr<T2>();

    BOOST_TEST_NE(t2, t1);
    BOOST_TEST(t2 != t1.type_info());
    BOOST_TEST(t2.type_info() != t1);

    BOOST_TEST(t1 < t2 || t2 < t1);
    BOOST_TEST(t1 > t2 || t2 > t1);
    BOOST_TEST(t1.type_info() < t2 || t2.type_info() < t1);
    BOOST_TEST(t1.type_info() > t2 || t2.type_info() > t1);
    BOOST_TEST(t1 < t2.type_info() || t2 < t1.type_info());
    BOOST_TEST(t1 > t2.type_info() || t2 > t1.type_info());

    // Chaecking that comparison operators overloads compile
    BOOST_TEST(t1 <= t2 || t2 <= t1);
    BOOST_TEST(t1 >= t2 || t2 >= t1);
    BOOST_TEST(t1.type_info() <= t2 || t2.type_info() <= t1);
    BOOST_TEST(t1.type_info() >= t2 || t2.type_info() >= t1);
    BOOST_TEST(t1 <= t2.type_info() || t2 <= t1.type_info());
    BOOST_TEST(t1 >= t2.type_info() || t2 >= t1.type_info());

    BOOST_TEST_EQ(t1, type_id_with_cvr<T1>());
    BOOST_TEST_EQ(t2, type_id_with_cvr<T2>());
    BOOST_TEST(t1 == type_id_with_cvr<T1>().type_info());
    BOOST_TEST(t2 == type_id_with_cvr<T2>().type_info());
    BOOST_TEST(t1.type_info() == type_id_with_cvr<T1>());
    BOOST_TEST(t2.type_info() == type_id_with_cvr<T2>());

    BOOST_TEST_EQ(t1.hash_code(), type_id_with_cvr<T1>().hash_code());
    BOOST_TEST_EQ(t2.hash_code(), type_id_with_cvr<T2>().hash_code());

    BOOST_TEST_NE(t1.hash_code(), type_id_with_cvr<T2>().hash_code());
    BOOST_TEST_NE(t2.hash_code(), type_id_with_cvr<T1>().hash_code());
}

void type_id_storing_modifiers()
{
    test_with_modofiers<int, const int>();
    test_with_modofiers<int, const int&>();
    test_with_modofiers<int, int&>();
    test_with_modofiers<int, volatile int>();
    test_with_modofiers<int, volatile int&>();
    test_with_modofiers<int, const volatile int>();
    test_with_modofiers<int, const volatile int&>();

    test_with_modofiers<const int, int>();
    test_with_modofiers<const int, const int&>();
    test_with_modofiers<const int, int&>();
    test_with_modofiers<const int, volatile int>();
    test_with_modofiers<const int, volatile int&>();
    test_with_modofiers<const int, const volatile int>();
    test_with_modofiers<const int, const volatile int&>();

    test_with_modofiers<const int&, int>();
    test_with_modofiers<const int&, const int>();
    test_with_modofiers<const int&, int&>();
    test_with_modofiers<const int&, volatile int>();
    test_with_modofiers<const int&, volatile int&>();
    test_with_modofiers<const int&, const volatile int>();
    test_with_modofiers<const int&, const volatile int&>();

    test_with_modofiers<int&, const int>();
    test_with_modofiers<int&, const int&>();
    test_with_modofiers<int&, int>();
    test_with_modofiers<int&, volatile int>();
    test_with_modofiers<int&, volatile int&>();
    test_with_modofiers<int&, const volatile int>();
    test_with_modofiers<int&, const volatile int&>();

#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    test_with_modofiers<int&&, const int>();
    test_with_modofiers<int&&, const int&>();
    test_with_modofiers<int&&, const int&&>();
    test_with_modofiers<int&&, int>();
    test_with_modofiers<int&&, volatile int>();
    test_with_modofiers<int&&, volatile int&>();
    test_with_modofiers<int&&, volatile int&&>();
    test_with_modofiers<int&&, const volatile int>();
    test_with_modofiers<int&&, const volatile int&>();
    test_with_modofiers<int&&, const volatile int&&>();
#endif
}

template <class T>
static void test_storing_nonstoring_modifiers_templ() {
    using namespace boost::typeindex;

    type_index t1 = type_id_with_cvr<T>();
    type_index t2 = type_id<T>();

    BOOST_TEST_EQ(t2, t1);
    BOOST_TEST_EQ(t1, t2);
    BOOST_TEST(t1 <= t2);
    BOOST_TEST(t1 >= t2);
    BOOST_TEST(t2 <= t1);
    BOOST_TEST(t2 >= t1);

    BOOST_TEST_EQ(t2.pretty_name(), t1.pretty_name());
}

void type_id_storing_modifiers_vs_nonstoring()
{
    test_storing_nonstoring_modifiers_templ<int>();
    test_storing_nonstoring_modifiers_templ<my_namespace1::my_class>();
    test_storing_nonstoring_modifiers_templ<my_namespace2::my_class>();

    boost::typeindex::type_index t1 = boost::typeindex::type_id_with_cvr<const int>();
    boost::typeindex::type_index t2 = boost::typeindex::type_id<int>();
    BOOST_TEST_NE(t2, t1);
    BOOST_TEST(t1.pretty_name() == "const int" || t1.pretty_name() == "int const");
}

void type_index_stream_operator_via_lexical_cast_testing()
{
    using namespace boost::typeindex;

    std::string s_int2 = boost::lexical_cast<std::string>(type_id<int>());
    BOOST_TEST_EQ(s_int2, "int");

    std::string s_double2 = boost::lexical_cast<std::string>(type_id<double>());
    BOOST_TEST_EQ(s_double2, "double");
}

void type_index_stripping_cvr_test()
{
    using namespace boost::typeindex;

    BOOST_TEST_EQ(type_id<int>(), type_id<const int>());
    BOOST_TEST_EQ(type_id<int>(), type_id<const volatile int>());
    BOOST_TEST_EQ(type_id<int>(), type_id<const volatile int&>());

    BOOST_TEST_EQ(type_id<int>(), type_id<int&>());
    BOOST_TEST_EQ(type_id<int>(), type_id<volatile int>());
    BOOST_TEST_EQ(type_id<int>(), type_id<volatile int&>());


    BOOST_TEST_EQ(type_id<double>(), type_id<const double>());
    BOOST_TEST_EQ(type_id<double>(), type_id<const volatile double>());
    BOOST_TEST_EQ(type_id<double>(), type_id<const volatile double&>());

    BOOST_TEST_EQ(type_id<double>(), type_id<double&>());
    BOOST_TEST_EQ(type_id<double>(), type_id<volatile double>());
    BOOST_TEST_EQ(type_id<double>(), type_id<volatile double&>());
}


void type_index_user_defined_class_test()
{
    using namespace boost::typeindex;

    BOOST_TEST_EQ(type_id<my_namespace1::my_class>(), type_id<my_namespace1::my_class>());
    BOOST_TEST_EQ(type_id<my_namespace2::my_class>(), type_id<my_namespace2::my_class>());

#ifndef BOOST_NO_RTTI
    BOOST_TEST(type_id<my_namespace1::my_class>() == typeid(my_namespace1::my_class));
    BOOST_TEST(type_id<my_namespace2::my_class>() == typeid(my_namespace2::my_class));
    BOOST_TEST(typeid(my_namespace1::my_class) == type_id<my_namespace1::my_class>());
    BOOST_TEST(typeid(my_namespace2::my_class) == type_id<my_namespace2::my_class>());
#endif

    BOOST_TEST_NE(type_id<my_namespace1::my_class>(), type_id<my_namespace2::my_class>());
    BOOST_TEST_NE(
        type_id<my_namespace1::my_class>().pretty_name().find("my_namespace1::my_class"),
        std::string::npos);
}





struct A {
public:
    BOOST_TYPE_INDEX_REGISTER_CLASS
    virtual ~A(){}
};

struct B: public A {
    BOOST_TYPE_INDEX_REGISTER_CLASS
};

struct C: public B {
    BOOST_TYPE_INDEX_REGISTER_CLASS
};

void comparators_type_id_runtime()
{
    C c1;
    B b1;
    A* pc1 = &c1;
    A& rc1 = c1;
    A* pb1 = &b1;
    A& rb1 = b1;

#ifndef BOOST_NO_RTTI
    BOOST_TEST(typeid(rc1) == typeid(*pc1));
    BOOST_TEST(typeid(rb1) == typeid(*pb1));

    BOOST_TEST(typeid(rc1) != typeid(*pb1));
    BOOST_TEST(typeid(rb1) != typeid(*pc1));

    BOOST_TEST(typeid(&rc1) == typeid(pb1));
    BOOST_TEST(typeid(&rb1) == typeid(pc1));
#else
    BOOST_TEST(boost::typeindex::type_index(pc1->boost_type_index_type_id_runtime_()).raw_name());
#endif

    BOOST_TEST_EQ(boost::typeindex::type_id_runtime(rc1), boost::typeindex::type_id_runtime(*pc1));
    BOOST_TEST_EQ(boost::typeindex::type_id<C>(), boost::typeindex::type_id_runtime(*pc1));
    BOOST_TEST_EQ(boost::typeindex::type_id_runtime(rb1), boost::typeindex::type_id_runtime(*pb1));
    BOOST_TEST_EQ(boost::typeindex::type_id<B>(), boost::typeindex::type_id_runtime(*pb1));

    BOOST_TEST_NE(boost::typeindex::type_id_runtime(rc1), boost::typeindex::type_id_runtime(*pb1));
    BOOST_TEST_NE(boost::typeindex::type_id_runtime(rb1), boost::typeindex::type_id_runtime(*pc1));

#ifndef BOOST_NO_RTTI
    BOOST_TEST_EQ(boost::typeindex::type_id_runtime(&rc1), boost::typeindex::type_id_runtime(pb1));
    BOOST_TEST_EQ(boost::typeindex::type_id_runtime(&rb1), boost::typeindex::type_id_runtime(pc1));

    BOOST_TEST(boost::typeindex::type_id_runtime(rc1) == typeid(*pc1));
    BOOST_TEST(boost::typeindex::type_id_runtime(rb1) == typeid(*pb1));

    BOOST_TEST(boost::typeindex::type_id_runtime(rc1) != typeid(*pb1));
    BOOST_TEST(boost::typeindex::type_id_runtime(rb1) != typeid(*pc1));
    BOOST_TEST(boost::typeindex::type_id_runtime(&rc1) == typeid(pb1));
    BOOST_TEST(boost::typeindex::type_id_runtime(&rb1) == typeid(pc1));
#endif
}


#ifndef BOOST_NO_RTTI

void comparators_type_id_vs_type_info()
{
    using namespace boost::typeindex;
    type_index t_int = type_id<int>();

    BOOST_TEST(t_int == typeid(int));
    BOOST_TEST(typeid(int) == t_int);
    BOOST_TEST(t_int <= typeid(int));
    BOOST_TEST(typeid(int) <= t_int);
    BOOST_TEST(t_int >= typeid(int));
    BOOST_TEST(typeid(int) >= t_int);

    type_index t_double = type_id<double>();

    BOOST_TEST(t_double == typeid(double));
    BOOST_TEST(typeid(double) == t_double);
    BOOST_TEST(t_double <= typeid(double));
    BOOST_TEST(typeid(double) <= t_double);
    BOOST_TEST(t_double >= typeid(double));
    BOOST_TEST(typeid(double) >= t_double);

    if (t_double < t_int) {
        BOOST_TEST(t_double < typeid(int));
        BOOST_TEST(typeid(double) < t_int);
        BOOST_TEST(typeid(int) > t_double);
        BOOST_TEST(t_int > typeid(double));


        BOOST_TEST(t_double <= typeid(int));
        BOOST_TEST(typeid(double) <= t_int);
        BOOST_TEST(typeid(int) >= t_double);
        BOOST_TEST(t_int >= typeid(double));
    } else {
        BOOST_TEST(t_double > typeid(int));
        BOOST_TEST(typeid(double) > t_int);
        BOOST_TEST(typeid(int) < t_double);
        BOOST_TEST(t_int < typeid(double));


        BOOST_TEST(t_double >= typeid(int));
        BOOST_TEST(typeid(double) >= t_int);
        BOOST_TEST(typeid(int) <= t_double);
        BOOST_TEST(t_int <= typeid(double));
    }

}

#endif // BOOST_NO_RTTI

int main() {
    names_matches_type_id();
    default_construction();
    copy_construction();
    comparators_type_id();
    hash_code_type_id();

    type_id_storing_modifiers();
    type_id_storing_modifiers_vs_nonstoring();
    type_index_stream_operator_via_lexical_cast_testing();
    type_index_stripping_cvr_test();
    type_index_user_defined_class_test();

    comparators_type_id_runtime();
#ifndef BOOST_NO_RTTI
    comparators_type_id_vs_type_info();
#endif

    return boost::report_errors();
}

