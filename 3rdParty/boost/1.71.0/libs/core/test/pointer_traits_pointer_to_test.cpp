/*
Copyright 2017 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/pointer_traits.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T>
class pointer {
public:
    typedef typename boost::pointer_traits<T>::element_type element_type;
    pointer(T value)
        : value_(value) { }
    T get() const BOOST_NOEXCEPT {
        return value_;
    }
    static pointer<T> pointer_to(element_type& value) {
        return pointer<T>(&value);
    }
private:
    T value_;
};

template<class T>
inline bool
operator==(const pointer<T>& lhs, const pointer<T>& rhs) BOOST_NOEXCEPT
{
    return lhs.get() == rhs.get();
}

int main()
{
    int i = 0;
    {
        typedef int* type;
        type p = &i;
        BOOST_TEST(boost::pointer_traits<type>::pointer_to(i) == p);
    }
    {
        typedef pointer<int*> type;
        type p(&i);
        BOOST_TEST(boost::pointer_traits<type>::pointer_to(i) == p);
    }
    {
        typedef pointer<pointer<int*> > type;
        type p(&i);
        BOOST_TEST(boost::pointer_traits<type>::pointer_to(i) == p);
    }
    {
        typedef const int* type;
        type p = &i;
        BOOST_TEST(boost::pointer_traits<type>::pointer_to(i) == p);
    }
    {
        typedef pointer<const int*> type;
        type p(&i);
        BOOST_TEST(boost::pointer_traits<type>::pointer_to(i) == p);
    }
    return boost::report_errors();
}
