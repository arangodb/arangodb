//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/empty_base_optimization.hpp>

#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace detail {

class empty_base_optimization_test
    : public beast::unit_test::suite
{
public:
    template<class T>
    class test1
        : private empty_base_optimization<T>
    {
        using Base = empty_base_optimization<T>;
        void* m_p;
    public:
        explicit test1 (T const& t)
            : Base (t)
        {}

        T&       member()       {return Base::member();}
        T const& member() const {return Base::member();}
    };

    template<class T>
    class test2
    {
        void* m_p;
        T m_t;
    public:
        explicit test2 (T const& t)
            : m_t (t)
        {}

        T&       member()       {return m_t;}
        T const& member() const {return m_t;}
    };

    struct Empty
    {
        operator bool() {return true;}
    };

    static
    bool
    test_one()
    {
        test1<int> t1(1);
        test2<int> t2(2);
        static_assert(sizeof(t1) == sizeof(t2), "don't optimize for int");
        if(t1.member() != 1)
            return false;
        if(t2.member() != 2)
            return false;
        return true;
    }

    static
    bool
    test_two()
    {
        test1<Empty> t1((Empty()));
        test2<Empty> t2((Empty()));
        static_assert(sizeof(t1) < sizeof(t2), "do optimize for Empty");
        if(t1.member() != true)
            return false;
        if(t2.member() != true)
            return false;
        return true;
    }

    void
    run()
    {
        BEAST_EXPECT(test_one());
        BEAST_EXPECT(test_two());
        pass();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,empty_base_optimization);

} // detail
} // beast
} // boost
