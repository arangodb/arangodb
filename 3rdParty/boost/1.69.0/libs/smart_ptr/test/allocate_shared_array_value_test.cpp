/*
Copyright 2012-2015 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/make_shared.hpp>

template<class T = void>
struct creator {
    typedef T value_type;

    template<class U>
    struct rebind {
        typedef creator<U> other;
    };

    creator() { }

    template<class U>
    creator(const creator<U>&) { }

    T* allocate(std::size_t size) {
        return static_cast<T*>(::operator new(sizeof(T) * size));
    }

    void deallocate(T* ptr, std::size_t) {
        ::operator delete(ptr);
    }
};

template<class T, class U>
inline bool
operator==(const creator<T>&, const creator<U>&)
{
    return true;
}

template<class T, class U>
inline bool
operator!=(const creator<T>&, const creator<U>&)
{
    return false;
}

int main()
{
    {
        boost::shared_ptr<int[]> result =
            boost::allocate_shared<int[]>(creator<int>(), 4, 1);
        BOOST_TEST(result[0] == 1);
        BOOST_TEST(result[1] == 1);
        BOOST_TEST(result[2] == 1);
        BOOST_TEST(result[3] == 1);
    }
    {
        boost::shared_ptr<int[4]> result =
            boost::allocate_shared<int[4]>(creator<int>(), 1);
        BOOST_TEST(result[0] == 1);
        BOOST_TEST(result[1] == 1);
        BOOST_TEST(result[2] == 1);
        BOOST_TEST(result[3] == 1);
    }
    {
        boost::shared_ptr<const int[]> result =
            boost::allocate_shared<const int[]>(creator<>(), 4, 1);
        BOOST_TEST(result[0] == 1);
        BOOST_TEST(result[1] == 1);
        BOOST_TEST(result[2] == 1);
        BOOST_TEST(result[3] == 1);
    }
    {
        boost::shared_ptr<const int[4]> result =
            boost::allocate_shared<const int[4]>(creator<>(), 1);
        BOOST_TEST(result[0] == 1);
        BOOST_TEST(result[1] == 1);
        BOOST_TEST(result[2] == 1);
        BOOST_TEST(result[3] == 1);
    }
    return boost::report_errors();
}
