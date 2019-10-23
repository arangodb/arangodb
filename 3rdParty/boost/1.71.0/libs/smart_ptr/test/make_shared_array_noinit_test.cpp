/*
Copyright 2012-2015 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/align/is_aligned.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/make_shared.hpp>
#include <boost/smart_ptr/weak_ptr.hpp>
#include <boost/type_traits/alignment_of.hpp>

class type {
public:
    static unsigned instances;

    type()
        : value_(0.0) {
        ++instances;
    }

    ~type() {
        --instances;
    }

    void set(long double value) {
        value_ = value;
    }

    long double get() const {
        return value_;
    }

private:
    type(const type&);
    type& operator=(const type&);

    long double value_;
};

unsigned type::instances = 0;

int main()
{
    {
        boost::shared_ptr<int[]> result =
            boost::make_shared_noinit<int[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<int[3]> result =
            boost::make_shared_noinit<int[3]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<int[][2]> result =
            boost::make_shared_noinit<int[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<int[2][2]> result =
            boost::make_shared_noinit<int[2][2]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<const int[]> result =
            boost::make_shared_noinit<const int[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<const int[3]> result =
            boost::make_shared_noinit<const int[3]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<const int[][2]> result =
            boost::make_shared_noinit<const int[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<const int[2][2]> result =
            boost::make_shared_noinit<const int[2][2]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<int>::value));
    }
    {
        boost::shared_ptr<type[]> result =
            boost::make_shared_noinit<type[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 3);
        boost::weak_ptr<type[]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[3]> result =
            boost::make_shared_noinit<type[3]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 3);
        boost::weak_ptr<type[3]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[][2]> result =
            boost::make_shared_noinit<type[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 4);
        boost::weak_ptr<type[][2]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<type[2][2]> result =
            boost::make_shared_noinit<type[2][2]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 4);
        boost::weak_ptr<type[2][2]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[]> result =
            boost::make_shared_noinit<const type[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 3);
        boost::weak_ptr<const type[]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[3]> result =
            boost::make_shared_noinit<const type[3]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 3);
        boost::weak_ptr<const type[3]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[][2]> result =
            boost::make_shared_noinit<const type[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 4);
        boost::weak_ptr<const type[][2]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    {
        boost::shared_ptr<const type[2][2]> result =
            boost::make_shared_noinit<const type[2][2]>();
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result.use_count() == 1);
        BOOST_TEST(boost::alignment::is_aligned(result.get(),
            boost::alignment_of<type>::value));
        BOOST_TEST(type::instances == 4);
        boost::weak_ptr<const type[2][2]> other = result;
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    return boost::report_errors();
}
