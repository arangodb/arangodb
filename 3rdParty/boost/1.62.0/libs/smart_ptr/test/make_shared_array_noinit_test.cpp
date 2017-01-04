/*
(c) 2012-2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/type_traits/alignment_of.hpp>

class type {
public:
    static unsigned int instances;
    explicit type() {
        instances++;
    }
    ~type() {
        instances--;
    }
private:
    type(const type&);
    type& operator=(const type&);
};

unsigned int type::instances = 0;

int main()
{
    {
        boost::shared_ptr<int[]> a1 = boost::make_shared_noinit<int[]>(3);
        int* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<int>::value == 0);
    }
    {
        boost::shared_ptr<int[3]> a1 = boost::make_shared_noinit<int[3]>();
        int* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<int>::value == 0);
    }
    {
        boost::shared_ptr<int[][2]> a1 = boost::make_shared_noinit<int[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
    }
    {
        boost::shared_ptr<int[2][2]> a1 = boost::make_shared_noinit<int[2][2]>();
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
    }
    {
        boost::shared_ptr<const int[]> a1 = boost::make_shared_noinit<const int[]>(3);
        const int* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<int>::value == 0);
    }
    {
        boost::shared_ptr<const int[3]> a1 = boost::make_shared_noinit<const int[3]>();
        const int* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<int>::value == 0);
    }
    {
        boost::shared_ptr<const int[][2]> a1 = boost::make_shared_noinit<const int[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
    }
    {
        boost::shared_ptr<const int[2][2]> a1 = boost::make_shared_noinit<const int[2][2]>();
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
    }
    {
        boost::shared_ptr<type[]> a1 = boost::make_shared_noinit<type[]>(3);
        type* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<type>::value == 0);
        BOOST_TEST(type::instances == 3);
        boost::weak_ptr<type[]> w1 = a1;
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[3]> a1 = boost::make_shared_noinit<type[3]>();
        type* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<type>::value == 0);
        BOOST_TEST(type::instances == 3);
        boost::weak_ptr<type[3]> w1 = a1;
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[][2]> a1 = boost::make_shared_noinit<type[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[2][2]> a1 = boost::make_shared_noinit<type[2][2]>();
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[]> a1 = boost::make_shared_noinit<const type[]>(3);
        const type* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<type>::value == 0);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[3]> a1 = boost::make_shared_noinit<const type[3]>();
        const type* a2 = a1.get();
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(a2 != 0);
        BOOST_TEST(size_t(a2) % boost::alignment_of<type>::value == 0);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[][2]> a1 = boost::make_shared_noinit<const type[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[2][2]> a1 = boost::make_shared_noinit<const type[2][2]>();
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1.use_count() == 1);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }
    return boost::report_errors();
}
