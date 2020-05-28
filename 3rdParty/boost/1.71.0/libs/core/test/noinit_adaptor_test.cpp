/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/noinit_adaptor.hpp>
#include <boost/core/default_allocator.hpp>
#include <boost/core/lightweight_test.hpp>
#include <vector>

template<class T>
class creator
    : public boost::default_allocator<T> {
public:
    template<class U>
    struct rebind {
        typedef creator<U> other;
    };

    creator(int state)
        : state_(state) { }

    template<class U>
    creator(const creator<U>& other)
        : state_(other.state()) { }

    template<class U, class V>
    void construct(U*, const V&) {
        BOOST_ERROR("construct");
    }

    template<class U>
    void destroy(U*) {
        BOOST_ERROR("destroy");
    }

    int state() const {
        return state_;
    }

private:
    int state_;
};

template<class T, class U>
inline bool
operator==(const creator<T>& lhs, const creator<U>& rhs)
{
    return lhs.state() == rhs.state();
}

template<class T, class U>
inline bool
operator!=(const creator<T>& lhs, const creator<U>& rhs)
{
    return !(lhs == rhs);
}

class type {
public:
    type() { }

    type(int value)
        : value_(value) { }

    int value() const {
        return value_;
    }

private:
    int value_;
};

inline bool
operator==(const type& lhs, const type& rhs)
{
    return lhs.value() == rhs.value();
}

template<class A>
void test(const A& allocator)
{
    std::vector<typename A::value_type, A> v(allocator);
    v.push_back(1);
    BOOST_TEST(v.front() == 1);
    v.clear();
    v.resize(5);
    v.front() = 1;
}

int main()
{
    test(boost::noinit_adaptor<creator<int> >(1));
    test(boost::noinit_adaptor<creator<type> >(2));
    test(boost::noinit_adapt(creator<int>(3)));
    test(boost::noinit_adapt(creator<type>(4)));
    return boost::report_errors();
}
