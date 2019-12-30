//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/detail/decorator.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

class decorator_test : public unit_test::suite
{
public:
    struct req_t
    {
        bool pass_ = false;

        req_t() = default;

        ~req_t()
        {
            BEAST_EXPECT(pass_);
        }

        req_t(req_t&& other) noexcept
            : pass_(boost::exchange(other.pass_, true))
        {
        }

        void
        operator()(request_type&)
        {
            BEAST_EXPECT(! pass_);
            pass_ = true;
        }
    };

    struct res_t
    {
        bool pass_ = false;

        res_t() = default;

        ~res_t()
        {
            BEAST_EXPECT(pass_);
        }

        res_t(res_t&& other) noexcept
            : pass_(boost::exchange(other.pass_, true))
        {
        }

        void
        operator()(response_type&)
        {
            BEAST_EXPECT(! pass_);
            pass_ = true;
        }
    };

    struct both_t : res_t , req_t
    {
        using req_t::operator();
        using res_t::operator();
    };

    struct big_t : both_t
    {
        using both_t::operator();
        char buf[2048];
    };

    struct goldi // just right
    {
        struct incomplete;
        std::shared_ptr<incomplete> sp1;
        std::shared_ptr<incomplete> sp2;
        void* param;

        void operator()(request_type &) const
        {
        }
    };

    void
    dec_req(request_type&)
    {
    }

    void
    testDecorator()
    {
        request_type req;
        response_type res;

        decorator{}(req);
        decorator{}(res);
        decorator{req_t{}}(req);
        decorator{res_t{}}(res);

        {
            decorator d(both_t{});
            d(req);
            d(res);
        }

        {
            decorator d(big_t{});
            d(req);
            d(res);
        }

        {
            decorator d1{req_t{}};
            decorator d2{std::move(d1)};
            d2(req);
            decorator d3;
            d3 = std::move(d2);
        }

        {
            decorator d{goldi{}};
            bool is_inline = d.vtable_ ==
                decorator::vtable_impl<goldi, true>::get();
            BEAST_EXPECT(is_inline);
        }
    }

    void
    run() override
    {
        log << "sizeof(decorator)==" << sizeof(decorator) << "\n";
        testDecorator();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,decorator);

} // detail
} // websocket
} // beast
} // boost
