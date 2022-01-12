//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/storage_ptr.hpp>

#include <type_traits>

#include "test.hpp"
#include "test_suite.hpp"

//----------------------------------------------------------

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4702) // unreachable code
#endif

BOOST_JSON_NS_BEGIN

BOOST_STATIC_ASSERT(
    std::is_nothrow_move_constructible<storage_ptr>::value);

class storage_ptr_test
{
public:
    static value jv1;
    static value jv2;

    struct throwing
        : memory_resource
    {
        throwing()
        {
            throw std::exception{};
        }

        void*
        do_allocate(
            std::size_t,
            std::size_t) override
        {
            return nullptr;
        }

        void
        do_deallocate(
            void*,
            std::size_t,
            std::size_t) noexcept override
        {
        }

        bool
        do_is_equal(
            memory_resource const& mr) const noexcept override
        {
            return this == &mr;
        }
    };

    void
    testMembers()
    {
        auto const dsp = storage_ptr{};
        auto const usp =
            make_shared_resource<unique_resource>();

        // ~storage_ptr()
        {
            // implied
        }

        // storage_ptr()
        {
            storage_ptr sp;
            BOOST_TEST(sp.get());
        }

        // storage_ptr(storage_ptr&&)
        {
            storage_ptr sp1 = dsp;
            storage_ptr sp2(std::move(sp1));
            BOOST_TEST(sp1.get());
            BOOST_TEST(*sp2 == *dsp);
        }

        // storage_ptr(storage_ptr const&)
        {
            storage_ptr sp1 = dsp;
            storage_ptr sp2(sp1);
            BOOST_TEST(sp1 == sp2);
        }

        // operator=(storage_ptr&&)
        {
            storage_ptr sp1(dsp);
            storage_ptr sp2(usp);
            sp2 = std::move(sp1);
            BOOST_TEST(*sp2 == *dsp);
        }

        // operator=(storage_ptr const&)
        {
            storage_ptr sp1(dsp);
            storage_ptr sp2(usp);
            sp2 = sp1;
            BOOST_TEST(*sp1 == *sp2);
        }

        // get()
        {
            {
                storage_ptr sp(dsp);
                BOOST_TEST(sp.get() == dsp.get());
            }
        }

        // operator->()
        {
            storage_ptr sp(dsp);
            BOOST_TEST(sp.operator->() == dsp.get());
        }

        // operator*()
        {
            storage_ptr sp(dsp);
            BOOST_TEST(&sp.operator*() == dsp.get());
        }

        // exception in make_storage
        {
            BOOST_TEST_THROWS(
                make_shared_resource<throwing>(),
                std::exception);
        }
    }

    // https://github.com/boostorg/json/pull/182
    void
    testPull182()
    {
        struct other
        {
            virtual void f() {}
        };

        struct my_resource
            : other
            , memory_resource
        {
            void*
            do_allocate(
                std::size_t,
                std::size_t) override
            {
                return nullptr;
            }

            void
            do_deallocate(
                void*,
                std::size_t,
                std::size_t) noexcept override
            {
            }

            bool
            do_is_equal(
                memory_resource const&) const noexcept override
            {
                return true;
            }
        };

        my_resource mr;
        BOOST_TEST(storage_ptr(&mr).get() == &mr);
    }

    void
    testInitOrder()
    {
        BOOST_ASSERT(jv1.storage() == jv2.storage());
        BOOST_TEST(! jv1.storage().is_deallocate_trivial());
        BOOST_TEST(! jv1.storage().is_shared());
    }

    void
    run()
    {
        storage_ptr sp;

        testMembers();
        testPull182();
        testInitOrder();
    }
};

value storage_ptr_test::jv1 = {1, 2, 3};
value storage_ptr_test::jv2 = {1, 2, 3};

TEST_SUITE(storage_ptr_test, "boost.json.storage_ptr");

BOOST_JSON_NS_END

#ifdef _MSC_VER
#pragma warning(pop)
#endif
