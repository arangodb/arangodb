//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/handler_ptr.hpp>

#include <boost/beast/unit_test/suite.hpp>
#include <exception>
#include <memory>
#include <utility>

namespace boost {
namespace beast {

class handler_ptr_test : public beast::unit_test::suite
{
public:
    struct handler
    {
        std::unique_ptr<int> ptr;
        void operator()(bool& b) const
        {
            b = true;
        }
    };

    struct T
    {
        explicit T(handler const&)
        {
        }

        ~T()
        {
        }
    };

    void
    testCtorExcept()
    {
        struct U
        {
            explicit U(handler const&)
            {
                throw std::exception{};
            }
        };
        handler_ptr<T, handler> p1{handler{}};
        try
        {
            handler_ptr<U, handler> p2{handler{}};
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
        catch(...)
        {
            fail("", __FILE__, __LINE__);
        }
    }

    void
    testMoveExcept()
    {
        struct throwing_handler
        {
            throwing_handler() = default;
            throwing_handler(throwing_handler&&)
            {
                throw std::bad_alloc{};
            }
            void operator()() const
            {
            }
        };
        struct T
        {
            explicit T(throwing_handler const&) noexcept {}
        };
        try
        {
            throwing_handler h;
            handler_ptr<T, throwing_handler> p{std::move(h)};
            fail("", __FILE__, __LINE__);
        }
        catch (std::bad_alloc const&)
        {
            pass();
        }
    }

    void
    testInvoke()
    {
        handler_ptr<T, handler> p{handler{}};
        bool b = false;
        BEAST_EXPECT(p.has_value());
        p.invoke(std::ref(b));
        BEAST_EXPECT(! p.has_value());
        BEAST_EXPECT(b);
    }

    void
    run() override
    {
        testCtorExcept();
        testMoveExcept();
        testInvoke();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,handler_ptr);

} // beast
} // boost
