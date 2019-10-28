/*
Copyright 2017-2018 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/pointer_traits.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T>
class P1 {
public:
    explicit P1(T* p)
        : p_(p) { }
    T* operator->() const BOOST_NOEXCEPT {
        return p_;
    }
private:
    T* p_;
};

template<class T>
class P2 {
public:
    explicit P2(T* p)
        : p_(p) { }
    P1<T> operator->() const BOOST_NOEXCEPT {
        return p_;
    }
private:
    P1<T> p_;
};

#if !defined(BOOST_NO_CXX14_RETURN_TYPE_DEDUCTION)
template<class T>
class P3 {
public:
    explicit P3(T* p)
        : p_(p) { }
    T* get() const BOOST_NOEXCEPT {
        return p_;
    }
private:
    T* p_;
};

namespace boost {
template<class T>
struct pointer_traits<P3<T> > {
    static T* to_address(const P3<T>& p) BOOST_NOEXCEPT {
        return p.get();
    }
};
} /* boost */

template<class T>
class P4 {
public:
    explicit P4(T* p)
        : p_(p) { }
    T* operator->() const BOOST_NOEXCEPT {
        return 0;
    }
    T* get() const BOOST_NOEXCEPT {
        return p_;
    }
private:
    int* p_;
};

namespace boost {
template<class T>
struct pointer_traits<P4<T> > {
    static T* to_address(const P4<T>& p) BOOST_NOEXCEPT {
        return p.get();
    }
};
} /* boost */

#if !defined(BOOST_NO_CXX11_POINTER_TRAITS)
template<class T>
class P5 {
public:
    explicit P5(T* p)
        : p_(p) { }
    T* get() const BOOST_NOEXCEPT {
        return p_;
    }
private:
    T* p_;
};

namespace std {
template<class T>
struct pointer_traits<P5<T> > {
    static T* to_address(const P5<T>& p) BOOST_NOEXCEPT {
        return p.get();
    }
};
} /* std */

template<class T>
class P6 {
public:
    explicit P6(T* p)
        : p_(p) { }
    T* get() const BOOST_NOEXCEPT {
        return p_;
    }
private:
    T* p_;
};

namespace boost {
template<class T>
struct pointer_traits<P6<T> > {
    static T* to_address(const P6<T>& p) BOOST_NOEXCEPT {
        return p.get();
    }
};
} /* boost */

namespace std {
template<class T>
struct pointer_traits<P6<T> > {
    static T* to_address(const P6<T>& p) BOOST_NOEXCEPT {
        return 0;
    }
};
} /* std */
#endif
#endif

int main()
{
    int i = 0;
    BOOST_TEST(boost::to_address(&i) == &i);
    int* p = &i;
    BOOST_TEST(boost::to_address(p) == &i);
    P1<int> p1(&i);
    BOOST_TEST(boost::to_address(p1) == &i);
    P2<int> p2(&i);
    BOOST_TEST(boost::to_address(p2) == &i);
#if !defined(BOOST_NO_CXX14_RETURN_TYPE_DEDUCTION)
    P3<int> p3(&i);
    BOOST_TEST(boost::to_address(p3) == &i);
    P4<int> p4(&i);
    BOOST_TEST(boost::to_address(p4) == &i);
#if !defined(BOOST_NO_CXX11_POINTER_TRAITS)
    P5<int> p5(&i);
    BOOST_TEST(boost::to_address(p5) == &i);
    P6<int> p6(&i);
    BOOST_TEST(boost::to_address(p6) == &i);
#endif
#endif
    return boost::report_errors();
}
