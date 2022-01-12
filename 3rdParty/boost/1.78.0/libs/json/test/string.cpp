//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

// Test that header file is self-contained.
#include <boost/json/string.hpp>

#include <boost/json/monotonic_resource.hpp>
#include <boost/json/parse.hpp>

#include <numeric>
#include <sstream>
#include <string>
#include <stdint.h>
#include <unordered_set>

#include "test.hpp"
#include "test_suite.hpp"

BOOST_JSON_NS_BEGIN

BOOST_STATIC_ASSERT( std::is_nothrow_destructible<string>::value );
BOOST_STATIC_ASSERT( std::is_nothrow_move_constructible<string>::value );

class string_test
{
public:

#if BOOST_JSON_ARCH == 64
# define INIT1 { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n' }
# define INIT2 { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O' }
#elif BOOST_JSON_ARCH == 32
# define INIT1 { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j' }
# define INIT2 { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K' }
#else
# error Unknown architecture
#endif

    struct test_vectors
    {
        // fit in sbo
        string_view v1; // "abc...

        // dynamic alloc
        string_view v2; // "ABC...

        std::string const s1;
        std::string const s2;

        test_vectors()
            : s1([&]
            {
                std::string s;
                s.resize(string{}.capacity());
                std::iota(s.begin(), s.end(), 'a');
                return s;
            }())
            , s2([&]
            {
                std::string s;
                s.resize(string{}.capacity() + 1);
                std::iota(s.begin(), s.end(), 'A');
                return s;
            }())
        {
            v1 = s1;
            v2 = s2;

            BOOST_TEST(std::string(INIT1) == s1);
            BOOST_TEST(std::string(INIT2) == s2);
        }
    };

    static
    string_view
    last_of(
        string_view s,
        std::size_t n)
    {
        return s.substr(s.size() - n);
    }

    void
    testConstruction()
    {
        test_vectors const t;

        // string()
        {
            string s;
        }

        // string(storage_ptr)
        {
            auto const sp =
                make_shared_resource<unique_resource>();
            string s(sp);
            BOOST_TEST(s.empty());
            BOOST_TEST(*s.storage() == *sp.get());
        }

        // string(size_type, char, storage_ptr)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                BOOST_TEST(s == std::string(t.v1.size(), '*'));
            });

            {
                string s(t.v2.size(), '*');
                BOOST_TEST(s == std::string(t.v2.size(), '*'));
            }
        }

        // string(char const*, storage_ptr)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.s1.c_str(), sp);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.s2.c_str(), sp);
                BOOST_TEST(s == t.v2);
            });

            {
                string s(t.s1.c_str());
                BOOST_TEST(s == t.v1);
            }

            {
                string s(t.s2.c_str());
                BOOST_TEST(s == t.v2);
            }
        }

        // string(char const*, size_type, storage_ptr)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.s1.c_str(), 3, sp);
                BOOST_TEST(s == "abc");
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.s2.c_str(), 3, sp);
                BOOST_TEST(s == "ABC");
            });

            {
                string s(t.s1.c_str(), 3);
                BOOST_TEST(s == "abc");
            }

            {
                string s(t.s2.c_str(), 3);
                BOOST_TEST(s == "ABC");
            }
        }

        // string(InputIt, InputIt, storage_ptr)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.begin(), t.v1.end(), sp);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.begin(), t.v2.end(), sp);
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(
                    make_input_iterator(t.v1.begin()),
                    make_input_iterator(t.v1.end()), sp);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(
                    make_input_iterator(t.v2.begin()),
                    make_input_iterator(t.v2.end()), sp);
                BOOST_TEST(s == t.v2);
            });

            {
                string s(t.v1.begin(), t.v1.end());
                BOOST_TEST(s == t.v1);
            }

            {
                string s(t.v2.begin(), t.v2.end());
                BOOST_TEST(s == t.v2);
            }

            {
                string s(
                    make_input_iterator(t.v1.begin()),
                    make_input_iterator(t.v1.end()));
                BOOST_TEST(s == t.v1);
            }

            {
                string s(
                    make_input_iterator(t.v2.begin()),
                    make_input_iterator(t.v2.end()));
                BOOST_TEST(s == t.v2);
            }
        }

        // string(string)
        {
            {
                string const s0(t.v1);
                string s(s0);
                BOOST_TEST(s == t.v1);
            }

            {
                string const s0(t.v2);
                string s(s0);
                BOOST_TEST(s == t.v2);
            }
        }

        // string(string, storage_ptr)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string const s0(t.v1);
                string s(s0, sp);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string const s0(t.v2);
                string s(s0, sp);
                BOOST_TEST(s == t.v2);
            });
        }

        // string(pilfered<string>)
        {
            {
                string s1(t.v1);
                string s2(pilfer(s1));
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(s1.empty());
                BOOST_TEST(
                    s1.storage() == storage_ptr());
            }

            {
                string s1(t.v2);
                string s2(pilfer(s1));
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(s1.empty());
                BOOST_TEST(
                    s1.storage() == storage_ptr());
            }

            // ensure pilfered-from objects
            // are trivially destructible
            {
                string s1(make_shared_resource<
                    monotonic_resource>());
                string s2(pilfer(s1));
                BOOST_TEST(s1.storage().get() ==
                    storage_ptr().get());
            }
        }

        // string(string&&)
        {
            {
                string s1(t.v1);
                string s2(std::move(s1));
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(s1.empty());
                BOOST_TEST(
                    *s1.storage() ==
                    *s2.storage());
            }

            {
                string s1(t.v2);
                string s2(std::move(s1));
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(s1.empty());
                BOOST_TEST(
                    *s1.storage() ==
                    *s2.storage());
            }
        }

        // string(string&&, storage_ptr)
        {
            // same storage

            fail_loop([&](storage_ptr const& sp)
            {
                string s1(t.v1, sp);
                string s2(std::move(s1), sp);
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(s1.empty());
                BOOST_TEST(
                    *s1.storage() ==
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s1(t.v2, sp);
                string s2(std::move(s1));
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(s1.empty());
                BOOST_TEST(
                    *s1.storage() ==
                    *s2.storage());
            });

            // different storage

            fail_loop([&](storage_ptr const& sp)
            {
                string s1(t.v1);
                string s2(std::move(s1), sp);
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(s1 == t.v1);
                BOOST_TEST(
                    *s1.storage() !=
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s1(t.v2);
                string s2(std::move(s1), sp);
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(s1 == t.v2);
                BOOST_TEST(
                    *s1.storage() !=
                    *s2.storage());
            });
        }

        // string(string_view, storage_ptr)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                BOOST_TEST(s == t.v2);
            });

            {
                string s(t.v1);
                BOOST_TEST(s == t.v1);
            }

            {
                string s(t.v2);
                BOOST_TEST(s == t.v2);
            }
        }
    }

    void
    testAssignment()
    {
        test_vectors const t;

        // operator=(string)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string const s2(t.v1);
                s = s2;
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string const s2(t.v1);
                s = s2;
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string const s2(t.v2);
                s = s2;
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string const s2(t.v2);
                s = s2;
                BOOST_TEST(s == t.v2);
            });
        }

        // operator=(string&&)
        {
            // same storage

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v1, sp);
                s = std::move(s2);
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v1, sp);
                s = std::move(s2);
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v2, sp);
                s = std::move(s2);
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v2, sp);
                s = std::move(s2);
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            // different storage

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v1);
                s = std::move(s2);
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v1);
                s = std::move(s2);
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v2);
                s = std::move(s2);
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v2);
                s = std::move(s2);
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });
        }

        // operator=(char const*)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s = t.s1.c_str();
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s = t.s1.c_str();
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s = t.s2.c_str();
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s = t.s2.c_str();
                BOOST_TEST(s == t.v2);
            });
        }

        // operator=(string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s = t.v1;
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s = t.v1;
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s = t.v2;
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s = t.v2;
                BOOST_TEST(s == t.v2);
            });
        }
    }

    void
    testAssign()
    {
        test_vectors const t;

        // assign(size_type, char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), 'x', sp);
                s.assign(t.v1.size(), '*');
                BOOST_TEST(
                    s == std::string(t.v1.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), 'x', sp);
                s.assign(t.v1.size(), '*');
                BOOST_TEST(
                    s == std::string(t.v1.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), 'x', sp);
                s.assign(t.v2.size(), '*');
                BOOST_TEST(
                    s == std::string(t.v2.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), 'x', sp);
                s.assign(t.v2.size(), '*');
                BOOST_TEST(
                    s == std::string(t.v2.size(), '*'));
            });
        }

        // assign(string)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), 'x', sp);
                s.assign(string(t.v1.size(), '*'));
                BOOST_TEST(
                    s == std::string(t.v1.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), 'x', sp);
                s.assign(string(t.v1.size(), '*'));
                BOOST_TEST(
                    s == std::string(t.v1.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), 'x', sp);
                s.assign(string(t.v2.size(), '*'));
                BOOST_TEST(
                    s == std::string(t.v2.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), 'x', sp);
                s.assign(string(t.v2.size(), '*'));
                BOOST_TEST(
                    s == std::string(t.v2.size(), '*'));
            });

            // self-assign
            {
                string s(t.v1);
                s = static_cast<string const&>(s);
                BOOST_TEST(s == t.v1);
            }
        }

        // assign(string&&)
        {
            // same storage

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v1, sp);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v1, sp);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v2, sp);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v2, sp);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2.empty());
                BOOST_TEST(
                    *s.storage() ==
                    *s2.storage());
            });

            // different storage

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v1);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v1);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v1);
                BOOST_TEST(s2 == t.v1);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v1.size(), '*');
                string s(c, sp);
                string s2(t.v2);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });

            fail_loop([&](storage_ptr const& sp)
            {
                std::string c(t.v2.size(), '*');
                string s(c, sp);
                string s2(t.v2);
                s.assign(std::move(s2));
                BOOST_TEST(s == t.v2);
                BOOST_TEST(s2 == t.v2);
                BOOST_TEST(
                    *s.storage() !=
                    *s2.storage());
            });
        }

        // assign(char const*, size_type)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.s1.c_str(), 3);
                BOOST_TEST(s == "abc");
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.s1.c_str(), 3);
                BOOST_TEST(s == "abc");
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.s2.c_str(), 3);
                BOOST_TEST(s == "ABC");
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.s2.c_str(), 3);
                BOOST_TEST(s == "ABC");
            });
        };

        // assign(char const* s)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.s1.c_str());
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.s1.c_str());
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.s2.c_str());
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.s2.c_str());
                BOOST_TEST(s == t.v2);
            });
        }

        // assign(InputIt, InputIt)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.s1.begin(), t.s1.end());
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.s1.begin(), t.s1.end());
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(
                    make_input_iterator(t.s1.begin()),
                    make_input_iterator(t.s1.end()));
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(
                    make_input_iterator(t.s1.begin()),
                    make_input_iterator(t.s1.end()));
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.s2.begin(), t.s2.end());
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.s2.begin(), t.s2.end());
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(
                    make_input_iterator(t.s2.begin()),
                    make_input_iterator(t.s2.end()));
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(
                    make_input_iterator(t.s2.begin()),
                    make_input_iterator(t.s2.end()));
                BOOST_TEST(s == t.v2);
            });

            // empty range
            {
                string s(t.v1);
                s.assign(t.s1.begin(), t.s1.begin());
                BOOST_TEST(s.empty());
            }

            // empty range
            {
                string s(t.v1);
                s.assign(
                    make_input_iterator(t.s1.begin()),
                    make_input_iterator(t.s1.begin()));
                BOOST_TEST(s.empty());
            }
        }

        // assign(string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.v1);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.v1);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1.size(), '*', sp);
                s.assign(t.v2);
                BOOST_TEST(s == t.v2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2.size(), '*', sp);
                s.assign(t.v2);
                BOOST_TEST(s == t.v2);
            });
        }
    }

    void
    testConversion()
    {
        test_vectors const t;

        string s(t.s1);
        auto const& cs(s);

        char const* chars = cs.c_str();
        BOOST_TEST(chars == t.s1);
        json::string_view sv = cs;
        BOOST_TEST(sv == t.s1);
#if ! defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
        std::string_view std_sv = cs;
        BOOST_TEST(std_sv == t.s1);
#endif

        s = t.s2;

        chars = cs.c_str();
        BOOST_TEST(chars == t.s2);
        sv = cs;
        BOOST_TEST(sv == t.s2);
#if ! defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
        std_sv = cs;
        BOOST_TEST(std_sv == t.s2);
#endif
    }

    void
    testElementAccess()
    {
        test_vectors const t;

        string s1(t.v1);
        string s2(t.v2);
        auto const& cs1(s1);
        auto const& cs2(s2);

        // at(size_type)
        {
            BOOST_TEST(s1.at(1) == 'b');
            s1.at(1) = '*';
            BOOST_TEST(s1.at(1) == '*');
            s1.at(1) = 'b';
            BOOST_TEST(s1.at(1) == 'b');

            BOOST_TEST(s2.at(1) == 'B');
            s2.at(1) = '*';
            BOOST_TEST(s2.at(1) == '*');
            s2.at(1) = 'B';
            BOOST_TEST(s2.at(1) == 'B');

            BOOST_TEST_THROWS(s1.at(s2.size()),
                std::out_of_range);
        }

        // at(size_type) const
        {
            BOOST_TEST(cs1.at(1) == 'b');
            BOOST_TEST(cs2.at(1) == 'B');

            BOOST_TEST_THROWS(cs1.at(cs2.size()),
                std::out_of_range);
        }

        // operator[&](size_type)
        {
            BOOST_TEST(s1[1] == 'b');
            s1[1] = '*';
            BOOST_TEST(s1[1] == '*');
            s1[1] = 'b';
            BOOST_TEST(s1[1] == 'b');

            BOOST_TEST(s2[1] == 'B');
            s2[1] = '*';
            BOOST_TEST(s2[1] == '*');
            s2[1] = 'B';
            BOOST_TEST(s2[1] == 'B');
        }

        // operator[&](size_type) const
        {
            BOOST_TEST(cs1[1] == 'b');
            BOOST_TEST(cs2[1] == 'B');
        }

        // front()
        {
            BOOST_TEST(s1.front() == 'a');
            s1.front() = '*';
            BOOST_TEST(s1.front() == '*');
            s1.front() = 'a';
            BOOST_TEST(s1.front() == 'a');

            BOOST_TEST(s2.front() == 'A');
            s2.front() = '*';
            BOOST_TEST(s2.front() == '*');
            s2.front() = 'A';
            BOOST_TEST(s2.front() == 'A');
        }

        // front() const
        {
            BOOST_TEST(cs1.front() == 'a');
            BOOST_TEST(cs2.front() == 'A');
        }

        // back()
        {
            auto const ch1 = s1.at(s1.size()-1);
            auto const ch2 = s2.at(s2.size()-1);

            BOOST_TEST(s1.back() == ch1);
            s1.back() = '*';
            BOOST_TEST(s1.back() == '*');
            s1.back() = ch1;
            BOOST_TEST(s1.back() == ch1);

            BOOST_TEST(s2.back() == ch2);
            s2.back() = '*';
            BOOST_TEST(s2.back() == '*');
            s2.back() = ch2;
            BOOST_TEST(s2.back() == ch2);
        }

        // back() const
        {
            auto const ch1 = s1.at(s1.size()-1);
            auto const ch2 = s2.at(s2.size()-1);

            BOOST_TEST(cs1.back() == ch1);
            BOOST_TEST(cs2.back() == ch2);
        }

        // data()
        {
            BOOST_TEST(
                string_view(s1.data()) == t.v1);
            BOOST_TEST(
                string_view(s2.data()) == t.v2);
        }
        // data() const
        {
            BOOST_TEST(
                string_view(cs1.data()) == t.v1);
            BOOST_TEST(
                string_view(cs2.data()) == t.v2);
        }

        // c_str()
        {
            BOOST_TEST(
                string_view(cs1.c_str()) == t.v1);
            BOOST_TEST(
                string_view(cs2.c_str()) == t.v2);
        }

        // operator string_view()
        {
            BOOST_TEST(
                string_view(cs1) == t.v1);
            BOOST_TEST(
                string_view(cs2) == t.v2);
        }
    }

    void
    testIterators()
    {
        string s = "abc";
        auto const& ac(s);

        {
            auto it = s.begin();
            BOOST_TEST(*it == 'a'); ++it;
            BOOST_TEST(*it == 'b');   it++;
            BOOST_TEST(*it == 'c'); ++it;
            BOOST_TEST(it == s.end());
        }
        {
            auto it = s.cbegin();
            BOOST_TEST(*it == 'a'); ++it;
            BOOST_TEST(*it == 'b');   it++;
            BOOST_TEST(*it == 'c'); ++it;
            BOOST_TEST(it == s.cend());
        }
        {
            auto it = ac.begin();
            BOOST_TEST(*it == 'a'); ++it;
            BOOST_TEST(*it == 'b');   it++;
            BOOST_TEST(*it == 'c'); ++it;
            BOOST_TEST(it == ac.end());
        }
        {
            auto it = s.end();
            --it; BOOST_TEST(*it == 'c');
            it--; BOOST_TEST(*it == 'b');
            --it; BOOST_TEST(*it == 'a');
            BOOST_TEST(it == s.begin());
        }
        {
            auto it = s.cend();
            --it; BOOST_TEST(*it == 'c');
            it--; BOOST_TEST(*it == 'b');
            --it; BOOST_TEST(*it == 'a');
            BOOST_TEST(it == s.cbegin());
        }
        {
            auto it = ac.end();
            --it; BOOST_TEST(*it == 'c');
            it--; BOOST_TEST(*it == 'b');
            --it; BOOST_TEST(*it == 'a');
            BOOST_TEST(it == ac.begin());
        }

        {
            auto it = s.rbegin();
            BOOST_TEST(*it == 'c'); ++it;
            BOOST_TEST(*it == 'b');   it++;
            BOOST_TEST(*it == 'a'); ++it;
            BOOST_TEST(it == s.rend());
        }
        {
            auto it = s.crbegin();
            BOOST_TEST(*it == 'c'); ++it;
            BOOST_TEST(*it == 'b');   it++;
            BOOST_TEST(*it == 'a'); ++it;
            BOOST_TEST(it == s.crend());
        }
        {
            auto it = ac.rbegin();
            BOOST_TEST(*it == 'c'); ++it;
            BOOST_TEST(*it == 'b');   it++;
            BOOST_TEST(*it == 'a'); ++it;
            BOOST_TEST(it == ac.rend());
        }
        {
            auto it = s.rend();
            --it; BOOST_TEST(*it == 'a');
            it--; BOOST_TEST(*it == 'b');
            --it; BOOST_TEST(*it == 'c');
            BOOST_TEST(it == s.rbegin());
        }
        {
            auto it = s.crend();
            --it; BOOST_TEST(*it == 'a');
            it--; BOOST_TEST(*it == 'b');
            --it; BOOST_TEST(*it == 'c');
            BOOST_TEST(it == s.crbegin());
        }
        {
            auto it = ac.rend();
            --it; BOOST_TEST(*it == 'a');
            it--; BOOST_TEST(*it == 'b');
            --it; BOOST_TEST(*it == 'c');
            BOOST_TEST(it == ac.rbegin());
        }

        {
            string s2;
            string const& cs2(s2);
            BOOST_TEST(std::distance(
                s2.begin(), s2.end()) == 0);
            BOOST_TEST(std::distance(
                cs2.begin(), cs2.end()) == 0);
            BOOST_TEST(std::distance(
                s2.rbegin(), s2.rend()) == 0);
            BOOST_TEST(std::distance(
                cs2.rbegin(), cs2.rend()) == 0);
        }
    }

    void
    testCapacity()
    {
        test_vectors const t;

        // empty()
        {
            {
                string s;
                BOOST_TEST(s.empty());
            }
            {
                string s = "abc";
                BOOST_TEST(! s.empty());
            }
        }

        // size()
        // max_size()
        {
            string s = "abc";
            BOOST_TEST(s.size() == 3);
            BOOST_TEST(s.max_size() < string::npos);
        }

        // reserve(size_type)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                s.append(t.v1);
                s.append(t.v2);

                s.reserve(0);
                BOOST_TEST(s.size() ==
                    t.v1.size() + t.v2.size());

                s.reserve(t.v1.size() + t.v2.size());
                BOOST_TEST(s.size() ==
                    t.v1.size() + t.v2.size());

                s.reserve(s.size() * 2);
                BOOST_TEST(s.capacity() >
                    t.v1.size() + t.v2.size());

                s.resize(t.v1.size());
                s.reserve(t.v1.size());
                BOOST_TEST(s == t.v1);
            });
        }

        // capacity()
        {
            // implied
        }

        // shrink_to_fit()
        fail_loop([&](storage_ptr const& sp)
        {
            string s(sp);
            string::size_type cap;

            cap = s.capacity();
            s.shrink_to_fit();
            BOOST_TEST(s.capacity() == cap);

            s.reserve(s.capacity() + 1);
            s.shrink_to_fit();
            BOOST_TEST(s.capacity() == cap);

            s.resize(cap * 3, '*');
            cap = s.capacity();
            s.resize(cap - 1);
            s.shrink_to_fit();
            BOOST_TEST(s.capacity() == cap);

            s.resize(cap / 2);
            BOOST_TEST(s.capacity() == cap);

            s.shrink_to_fit();
            BOOST_TEST(s.capacity() < cap);
        });
    }

    void
    testClear()
    {
        test_vectors const t;

        // clear()
        {
            {
                string s(t.v1);
                s.clear();
                BOOST_TEST(s.empty());
                BOOST_TEST(s.size() == 0);
                BOOST_TEST(s.capacity() > 0);
            }

            {
                string s(t.v2);
                s.clear();
                BOOST_TEST(s.empty());
                BOOST_TEST(s.size() == 0);
                BOOST_TEST(s.capacity() > 0);
            }
        }
    }

    void
    testInsert()
    {
        test_vectors const t;

        // insert(size_type, size_type, char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(1, 3, '*');
                BOOST_TEST(s == std::string(
                    t.v1).insert(1, 3, '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(1, 3, '*');
                BOOST_TEST(s == std::string(
                    t.v2).insert(1, 3, '*'));
            });

            // pos out of range
            {
                string s(t.v1);
                BOOST_TEST_THROWS(
                    (s.insert(s.size() + 2, 1, '*')),
                    std::out_of_range);
            }

            // size > max_size
            {
                string s(t.v1);
                BOOST_TEST_THROWS(
                    (s.insert(1, s.max_size(), 'a')),
                    std::length_error);
            }
        }

        // insert(size_type, char const*)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(1, "***");
                BOOST_TEST(s == std::string(
                    t.v1).insert(1, "***"));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(1, "***");
                BOOST_TEST(s == std::string(
                    t.v2).insert(1, "***"));
            });

            // pos out of range
            {
                string s(t.v1);
                BOOST_TEST_THROWS(
                    (s.insert(s.size() + 2, "*")),
                    std::out_of_range);
            }
        }

        // insert(size_type, char const*, size_type)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(1, "*****");
                BOOST_TEST(s == std::string(
                    t.v1).insert(1, "*****"));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(1, "*****");
                BOOST_TEST(s == std::string(
                    t.v2).insert(1, "*****"));
            });
        }

        // insert(size_type, string const&)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(1, string(t.v2));
                BOOST_TEST(s == std::string(
                    t.v1).insert(1, t.s2));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(1, string(t.v1));
                BOOST_TEST(s == std::string(
                    t.v2).insert(1, t.s1));
            });
        }

        //// KRYSTIAN These tests are superseded by the new string_view overloads
        //// insert(size_type, string const&, size_type, size_type)
        //{
        //    fail_loop([&](storage_ptr const& sp)
        //    {
        //        string s(t.v1, sp);
        //        s.insert(1, string(t.v2), 1, 3);
        //        BOOST_TEST(s == std::string(
        //            t.v1).insert(1, t.s2, 1, 3));
        //    });

        //    fail_loop([&](storage_ptr const& sp)
        //    {
        //        string s(t.v2, sp);
        //        s.insert(1, string(t.v1), 1, 3);
        //        BOOST_TEST(s == std::string(
        //            t.v2).insert(1, t.s1, 1, 3));
        //    });

        //    fail_loop([&](storage_ptr const& sp)
        //    {
        //        string s(t.v1, sp);
        //        s.insert(1, string(t.v2), 1);
        //        BOOST_TEST(s == std::string(
        //            t.v1).insert(1, t.s2, 1, std::string::npos));
        //    });

        //    fail_loop([&](storage_ptr const& sp)
        //    {
        //        string s(t.v2, sp);
        //        s.insert(1, string(t.v1), 1);
        //        BOOST_TEST(s == std::string(
        //            t.v2).insert(1, t.s1, 1, std::string::npos));
        //    });
        //}

        // insert(size_type, char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                BOOST_TEST(
                    s.insert(2, '*')[2] == '*');
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                BOOST_TEST(
                    s.insert(2, '*')[2] == '*');
            });
        }

        // insert(const_iterator, size_type, char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                BOOST_TEST(string_view(
                    &(s.insert(2, 3, '*')[2]), 5) ==
                        "***cd");
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                BOOST_TEST(string_view(
                    &(s.insert(2, 3, '*')[2]), 5) ==
                        "***CD");
            });
        }

        // insert(const_iterator, InputIt, InputIt)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(2, t.s2.begin(), t.s2.end());
                std::string cs(t.s1);
                cs.insert(2, &t.s2[0], t.s2.size());
                BOOST_TEST(s == cs);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(2, t.s1.begin(), t.s1.end());
                std::string cs(t.s2);
                cs.insert(2, &t.s1[0], t.s1.size());
                BOOST_TEST(s == cs);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(2,
                    make_input_iterator(t.s2.begin()),
                    make_input_iterator(t.s2.end()));
                std::string cs(t.s1);
                cs.insert(2, &t.s2[0], t.s2.size());
                BOOST_TEST(s == cs);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(2,
                    make_input_iterator(t.s1.begin()),
                    make_input_iterator(t.s1.end()));
                std::string cs(t.s2);
                cs.insert(2, &t.s1[0], t.s1.size());
                BOOST_TEST(s == cs);
            });
        }

        // insert(const_iterator, string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(2, string_view(t.v2));
                std::string cs(t.v1);
                cs.insert(2, t.s2);
                BOOST_TEST(s == cs);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(2, string_view(t.v1));
                std::string cs(t.v2);
                cs.insert(2, t.s1);
                BOOST_TEST(s == cs);
            });
        }

        // insert(const_iterator, string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(2, string_view(t.v2).substr(2, 3));
                std::string cs(t.v1);
                cs.insert(2, t.s2, 2, 3);
                BOOST_TEST(s == cs);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(2, string_view(t.v1).substr(2, 3));
                std::string cs(t.v2);
                cs.insert(2, t.s1, 2, 3);
                BOOST_TEST(s == cs);
            });
        }

        // insert(size_type, char const*)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.insert(1, "***");
                BOOST_TEST(s == std::string(
                    t.v1).insert(1, "***"));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.insert(1, "***");
                BOOST_TEST(s == std::string(
                    t.v2).insert(1, "***"));
            });

            // pos out of range
            {
                string s(t.v1);
                BOOST_TEST_THROWS(
                    (s.insert(s.size() + 2, "*")),
                    std::out_of_range);
            }
        }

        // insert tests for when source is within destination
        {
            // start before splice point
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.reserve(s.size() + 10);
                s.insert(4, s.subview(0, 3));
                std::string cs(t.v1);
                cs.insert(4, cs.substr(0, 3));
                BOOST_TEST(s == cs);
            });

            // start after splice point
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.reserve(s.size() + 10);
                s.insert(0, s.subview(3, 3));
                std::string cs(t.v1);
                cs.insert(0, cs.substr(3, 3));
                BOOST_TEST(s == cs);
            });

            // insert pos bisects the inserted string
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.reserve(s.size() + 10);
                s.insert(2, s.subview(0, 5));
                std::string cs(t.v1);
                cs.insert(2, cs.substr(0, 5));
                BOOST_TEST(s == cs);
            });
        }

        // insert reallocation test
        {
            fail_loop([&](storage_ptr const& sp)
            {
              string s(t.v2, sp);
              std::string cs(t.v2);
              const auto view = t.v2.substr(0, 4);
              s.append(s);
              cs.append(cs);
              s.insert(2, view);
              cs.insert(2, view.data(), view.size());
              BOOST_TEST(s == cs);
            });
        }
    }

    void
    testErase()
    {
        test_vectors const t;

        // erase(size_type, size_type)
        {
            {
                string s(t.v1);
                s.erase(1, 3);
                BOOST_TEST(s ==
                    std::string(t.v1).erase(1, 3));
            }

            {
                string s(t.v2);
                s.erase(1, 3);
                BOOST_TEST(s ==
                    std::string(t.v2).erase(1, 3));
            }

            {
                string s(t.v1);
                s.erase(3);
                BOOST_TEST(s ==
                    std::string(t.v1).erase(3));
            }

            {
                string s(t.v2);
                s.erase(3);
                BOOST_TEST(s ==
                    std::string(t.v2).erase(3));
            }

            {
                string s(t.v1);
                s.erase();
                BOOST_TEST(s ==
                    std::string(t.v1).erase());
            }

            {
                string s(t.v2);
                s.erase();
                BOOST_TEST(s ==
                    std::string(t.v2).erase());
            }

            {
                string s(t.v1);
                BOOST_TEST_THROWS(
                    (s.erase(t.v1.size() + 1, 1)),
                    std::out_of_range);
            }
        }

        // iterator erase(const_iterator)
        {
            {
                string s(t.v1);
                std::string s2(t.v1);
                s.erase(s.begin() + 3);
                s2.erase(s2.begin() + 3);
                BOOST_TEST(s == s2);
            }

            {
                string s(t.v2);
                std::string s2(t.v2);
                s.erase(s.begin() + 3);
                s2.erase(s2.begin() + 3);
                BOOST_TEST(s == s2);
            }
        }

        // iterator erase(const_iterator, const_iterator)
        {
            string s(t.v1);
            std::string s2(t.v1);
            s.erase(s.begin() + 1, s.begin() + 3);
            s2.erase(s2.begin() + 1, s2.begin() + 3);
            BOOST_TEST(s == s2);
        }
    }

    void
    testPushPop()
    {
        test_vectors const t;

        // push_back(char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                for(auto ch : t.v1)
                    s.push_back(ch);
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                for(auto ch : t.v2)
                    s.push_back(ch);
                BOOST_TEST(s == t.v2);
            });
        }

        // pop_back(char)
        {
            {
                string s(t.v1);
                while(! s.empty())
                    s.pop_back();
                BOOST_TEST(s.empty());
                BOOST_TEST(s.capacity() > 0);
            }

            {
                string s(t.v2);
                while(! s.empty())
                    s.pop_back();
                BOOST_TEST(s.empty());
                BOOST_TEST(s.capacity() > 0);
            }
        }
    }

    void
    testAppend()
    {
        test_vectors const t;

        // append(size_type, char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(t.v2.size(), '*');
                BOOST_TEST(s == t.s1 +
                    std::string(t.v2.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(t.v1.size(), '*');
                BOOST_TEST(s == t.s2 +
                    std::string(t.v1.size(), '*'));
            });
        }

        // append(string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(string(t.v2));
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(string(t.v1));
                BOOST_TEST(s == t.s2 + t.s1);
            });
        }

        // append(string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(string(t.v2).subview(3));
                BOOST_TEST(s == t.s1 + t.s2.substr(3));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(string(t.v1).subview(3));
                BOOST_TEST(s == t.s2 + t.s1.substr(3));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(string(t.v2).subview(2, 3));
                BOOST_TEST(s == t.s1 + t.s2.substr(2, 3));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(string(t.v1).subview(2, 3));
                BOOST_TEST(s == t.s2 + t.s1.substr(2, 3));
            });
        }

        // append(char const*)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(t.s2.c_str());
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(t.s1.c_str());
                BOOST_TEST(s == t.s2 + t.s1);
            });
        }

        // append(InputIt, InputIt)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(t.s2.begin(), t.s2.end());
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(t.s1.begin(), t.s1.end());
                BOOST_TEST(s == t.s2 + t.s1);
            });

            // Fails on Visual Studio 2017 C++2a Strict
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(
                    make_input_iterator(t.s2.begin()),
                    make_input_iterator(t.s2.end()));
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(
                    make_input_iterator(t.s1.begin()),
                    make_input_iterator(t.s1.end()));
                BOOST_TEST(s == t.s2 + t.s1);
            });
        }

        // append(string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(t.v2);
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(t.v1);
                BOOST_TEST(s == t.s2 + t.s1);
            });
        }

        // append(string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(t.v2.substr(2));
                BOOST_TEST(s == t.s1 + t.s2.substr(2));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(t.v1.substr(2));
                BOOST_TEST(s == t.s2 + t.s1.substr(2));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s.append(t.v2.substr(2, 3));
                BOOST_TEST(s == t.s1 + t.s2.substr(2, 3));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s.append(t.v1.substr(2, 3));
                BOOST_TEST(s == t.s2 + t.s1.substr(2, 3));
            });
        }
    }

    void
    testPlusEquals()
    {
        test_vectors const t;

        // operator+=(string)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s += string(t.v2);
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s += string(t.v1);
                BOOST_TEST(s == t.s2 + t.s1);
            });
        }

        // operator+=(char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                for(auto ch : t.v1)
                    s += ch;
                BOOST_TEST(s == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                for(auto ch : t.v2)
                    s += ch;
                BOOST_TEST(s == t.v2);
            });
        }

        // operator+=(char const*)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s += t.s2.c_str();
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s += t.s1.c_str();
                BOOST_TEST(s == t.s2 + t.s1);
            });
        }

        // operator+=(string_view)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v1, sp);
                s += t.v2;
                BOOST_TEST(s == t.s1 + t.s2);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                s += t.v1;
                BOOST_TEST(s == t.s2 + t.s1);
            });
        }
    }

    void
    testCompare()
    {
        test_vectors const t;
        string const v1 = t.v1;

        // compare(string)
        BOOST_TEST(v1.compare(string("aaaaaaa")) > 0);
        BOOST_TEST(v1.compare(string(t.v1)) == 0);
        BOOST_TEST(v1.compare(string("bbbbbbb")) < 0);

        // compare(char const*)
        BOOST_TEST(v1.compare("aaaaaaa") > 0);
        BOOST_TEST(v1.compare(t.s1.c_str()) == 0);
        BOOST_TEST(v1.compare("bbbbbbb") < 0);

        // compare(string_view s)
        BOOST_TEST(v1.compare(string_view("aaaaaaa")) > 0);
        BOOST_TEST(v1.compare(t.v1) == 0);
        BOOST_TEST(v1.compare(string_view("bbbbbbb")) < 0);
    }

    void
    testStartEndsWith()
    {
        test_vectors const t;
        string const v1 = t.v1;
        string const v2 = t.v2;

        // starts_with(string_view)
        {
            BOOST_TEST(v1.starts_with(string_view("abc")));
            BOOST_TEST(v2.starts_with(string_view("ABC")));
            BOOST_TEST(! v1.starts_with(string_view("xyz")));
            BOOST_TEST(! v2.starts_with(string_view("XYZ")));
        }

        // starts_with(char)
        {
            BOOST_TEST(v1.starts_with('a'));
            BOOST_TEST(v2.starts_with('A'));
            BOOST_TEST(! v1.starts_with('x'));
            BOOST_TEST(! v2.starts_with('X'));
        }

        // starts_with(char const*)
        {
            BOOST_TEST(v1.starts_with("abc"));
            BOOST_TEST(v2.starts_with("ABC"));
            BOOST_TEST(! v1.starts_with("xyz"));
            BOOST_TEST(! v2.starts_with("XYZ"));
        }

        // ends_with(string_view)
        {
            BOOST_TEST(v1.ends_with(last_of(t.s1,3)));
            BOOST_TEST(v2.ends_with(last_of(t.s2,3)));
            BOOST_TEST(! v1.ends_with(string_view("abc")));
            BOOST_TEST(! v2.ends_with(string_view("ABC")));
        }

        // ends_with(char)
        {
            BOOST_TEST(v1.ends_with(last_of(t.s1, 1)[0]));
            BOOST_TEST(v2.ends_with(last_of(t.s2, 1)[0]));
            BOOST_TEST(! v1.ends_with('a'));
            BOOST_TEST(! v2.ends_with('A'));
        }

        // ends_with(char const*)
        {
            BOOST_TEST(v1.ends_with(last_of(t.s1, 3).data()));
            BOOST_TEST(v2.ends_with(last_of(t.s2, 3).data()));
            BOOST_TEST(! v1.ends_with("abc"));
            BOOST_TEST(! v2.ends_with("ABC"));
        }
    }

    void
    testReplace()
    {
        test_vectors const t;

        // replace(std::size_t, std::size_t, string_view)
        {
            // pos out of range
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                BOOST_TEST_THROWS(s.replace(s.size() + 1, 1, t.v2),
                    std::out_of_range);
            });

            // outside, shrink
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(0, 4, t.v2.substr(4, 2)) ==
                           s1.replace(0, 4, t.v2.data() + 4, 2));
            });

            // outside, grow
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(0, 1, t.v2.substr(0)) ==
                           s1.replace(0, 1, t.v2.data(), t.v2.size()));
            });

            // outside, grow, reallocate
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                s1.append(s1);
                s2.append(s2);
                BOOST_TEST(s2.replace(0, 1, t.v2.substr(0)) ==
                           s1.replace(0, 1, t.v2.data(), t.v2.size()));
            });

            // outside, same
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(0, 2, t.v2.substr(0, 2)) ==
                           s1.replace(0, 2, t.v2.data(), 2));
            });

            // replace tests for full coverage

            // inside, no effect
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(0, 4, s2.subview(0, 4)) ==
                           s1.replace(0, 4, s1.data() + 0, 4));
            });

            // inside, shrink, split
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(1, 4, s2.subview(4, 2)) ==
                           s1.replace(1, 4, s1.data() + 4, 2));
            });

            // inside, grow no reallocate, split
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(1, 1, s2.subview(0)) ==
                           s1.replace(1, 1, s1.data(), s1.size()));
            });

            // inside, reallocate, split
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                s1.append(s1);
                s2.append(s2);
                BOOST_TEST(s2.replace(1, 1, s2.subview(0)) ==
                           s1.replace(1, 1, s1.data(), s1.size()));
            });

            // inside, same, split
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(1, 2, s2.subview(0, 2)) ==
                           s1.replace(1, 2, s1.data(), 2));
            });
        }

        // replace(const_iterator, const_iterator, string_view)
        {
            // outside, shrink
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(
                  s2.replace(
                      s2.begin(),
                      s2.begin() + 4,
                      t.v2.substr(4, 2)) ==
                  s1.replace(0,
                      4, t.v2.data() + 4,
                      2));
            });

            // outside, grow
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(
                    s2.replace(
                        s2.begin(),
                        s2.begin() + 1,
                        t.v2.substr(0)) ==
                    s1.replace(0,
                        1, t.v2.data(),
                        t.v2.size()));
            });

            // outside, same
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(
                    s2.replace(
                        s2.begin(),
                        s2.begin() + 2,
                        t.v2.substr(0, 2)) ==
                    s1.replace(
                        0, 2,
                        t.v2.data(),
                        2));
            });

            // inside, shrink
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(
                    s2.replace(
                        s2.begin() + 1,
                        s2.begin() + 5,
                        s2.subview(4, 2)) ==
                    s1.replace(
                        1, 4,
                        s1.data() + 4,
                        2));
            });

            // inside, grow
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(
                    s2.replace(
                        s2.begin() + 1,
                        s2.begin() + 2,
                        s2.subview(0)) ==
                    s1.replace(
                        1, 1,
                        s1.data(),
                        s1.size()));
            });

            // inside, same
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(
                    s2.replace(
                        s2.begin() + 1,
                        s2.begin() + 3,
                        s2.subview(0, 2)) ==
                    s1.replace(
                        1, 2,
                        s1.data(),
                        2));
            });
        }

        // replace(std::size_t, std::size_t, std::size_t, char)
        {
            // grow, no realloc
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(0, 4, 10, 'a') ==
                           s1.replace(0, 4, 10, 'a'));
            });

            // grow, realloc
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                const auto grow = (std::max)(s1.capacity(), s2.capacity());
                BOOST_TEST(s2.replace(0, 4, grow, 'a') ==
                           s1.replace(0, 4, grow, 'a'));
            });

            // no change in size
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(s2.replace(0, 1, 1, 'a') ==
                           s1.replace(0, 1, 1, 'a'));
            });

            // pos out of range
            fail_loop([&](storage_ptr const& sp)
            {
                string s(t.v2, sp);
                BOOST_TEST_THROWS(s.replace(s.size() + 1, 1, 1, 'a'),
                    std::out_of_range);
            });
        }

        // replace(const_iterator, const_iterator, std::size_t, char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                std::string s1(t.v2.data(), t.v2.size());
                string s2(t.v2, sp);
                BOOST_TEST(
                  s2.replace(s2.begin(), s2.begin() + 4, 10, 'a') ==
                  s1.replace(0, 4, 10, 'a'));
            });
        }
    }

    void
    testSubStr()
    {
        test_vectors const t;
        string const s1 = t.v1;
        string const s2 = t.v2;

        // subview(size_type, size_type)
        BOOST_TEST(s1.subview() == t.v1);
        BOOST_TEST(s1.subview(1) == t.v1.substr(1));
        BOOST_TEST(s1.subview(1, 3) == t.v1.substr(1, 3));
        BOOST_TEST(s2.subview() == t.v2);
        BOOST_TEST(s2.subview(1) == t.v2.substr(1));
        BOOST_TEST(s2.subview(1, 3) == t.v2.substr(1, 3));
    }

    void
    testCopy()
    {
        test_vectors const t;

        // copy(char*, count, pos)
        {
            {
                string s(t.v1);
                std::string d;
                d.resize(s.size());
                s.copy(&d[0], d.size(), 0);
                BOOST_TEST(d == t.v1);
            }

            {
                string s(t.v1);
                std::string d;
                d.resize(s.size());
                s.copy(&d[0], d.size());
                BOOST_TEST(d == t.v1);
            }
        }
    }

    void
    testResize()
    {
        test_vectors const t;

        // resize(size_type)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                s.resize(t.v1.size());
                BOOST_TEST(s.size() == t.v1.size());
                BOOST_TEST(s == string(t.v1.size(), '\0'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                s.resize(t.v2.size());
                BOOST_TEST(s.size() == t.v2.size());
                BOOST_TEST(s == string(t.v2.size(), '\0'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                s.resize(t.v1.size());
                s.resize(t.v2.size());
                BOOST_TEST(s == string(t.v2.size(), '\0'));
                s.resize(t.v1.size());
                BOOST_TEST(s == string(t.v1.size(), '\0'));
            });
        }

        // resize(size_type, char)
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                s.resize(t.v1.size(), '*');
                BOOST_TEST(s.size() == t.v1.size());
                BOOST_TEST(s == string(t.v1.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                s.resize(t.v2.size(), '*');
                BOOST_TEST(s.size() == t.v2.size());
                BOOST_TEST(s == string(t.v2.size(), '*'));
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s(sp);
                s.resize(t.v1.size(), '*');
                s.resize(t.v2.size(), '*');
                BOOST_TEST(s == string(t.v2.size(), '*'));
                s.resize(t.v1.size());
                BOOST_TEST(s == string(t.v1.size(), '*'));
            });
        }
    }

    void
    testSwap()
    {
        test_vectors const t;

        // swap
        {
            fail_loop([&](storage_ptr const& sp)
            {
                string s1(t.v1, sp);
                string s2(t.v2, sp);
                s1.swap(s2);
                BOOST_TEST(s1 == t.v2);
                BOOST_TEST(s2 == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s1(t.v1, sp);
                string s2(t.v2, sp);
                swap(s1, s2);
                BOOST_TEST(s1 == t.v2);
                BOOST_TEST(s2 == t.v1);
            });

            fail_loop([&](storage_ptr const& sp)
            {
                string s1(t.v1);
                string s2(t.v2, sp);
                s1.swap(s2);
                BOOST_TEST(s1 == t.v2);
                BOOST_TEST(s2 == t.v1);
            });
        }
    }

    void
    testFind()
    {
        test_vectors const t;
        string const s1 = t.v1;

        // find(string_view, size_type)
        BOOST_TEST(s1.find("bcd") == 1);
        BOOST_TEST(s1.find("cde") == 2);

        BOOST_TEST(s1.find("bcd", 0) == 1);
        BOOST_TEST(s1.find("cde", 1) == 2);
        BOOST_TEST(s1.find("efg", 5) == string::npos);

        // find(char, size_type)
        BOOST_TEST(s1.find('b') == 1);
        BOOST_TEST(s1.find('c', 1) == 2);
        BOOST_TEST(s1.find('e', 5) == string::npos);
    }

    void
    testRfind()
    {
        test_vectors const t;
        string const s1 = t.v1;

        // rfind(string_view, size_type)
        BOOST_TEST(s1.rfind("bcd") == 1);
        BOOST_TEST(s1.rfind("cde") == 2);

        BOOST_TEST(s1.rfind("bcd", 1) == 1);
        BOOST_TEST(s1.rfind("cde", 2) == 2);
        BOOST_TEST(s1.rfind("efg", 3) == string::npos);

        // rfind(char, size_type)
        BOOST_TEST(s1.rfind('b') == 1);
        BOOST_TEST(s1.rfind('c', 2) == 2);
        BOOST_TEST(s1.rfind('e', 3) == string::npos);
    }

    void
    testFindFirstOf()
    {
        test_vectors const t;
        string const s1 = t.v1;

        // find_first_of(string_view, size_type)
        BOOST_TEST(s1.find_first_of("bcd") == 1);
        BOOST_TEST(s1.find_first_of("cde") == 2);

        BOOST_TEST(s1.find_first_of("bcd", 0) == 1);
        BOOST_TEST(s1.find_first_of("cde", 1) == 2);
        BOOST_TEST(s1.find_first_of("efg", 7) == string::npos);
    }

    void
    testFindFirstNotOf()
    {
        test_vectors const t;
        string const s1 = t.v1;

        // find_first_not_of(string_view, size_type)
        BOOST_TEST(s1.find_first_not_of("abc") == 3);
        BOOST_TEST(s1.find_first_not_of("cde") == 0);

        BOOST_TEST(s1.find_first_not_of("bcd", 0) == 0);
        BOOST_TEST(s1.find_first_not_of("cde", 2) == 5);

        // find_first_not_of(char, size_type)
        BOOST_TEST(s1.find_first_not_of('b') == 0);
        BOOST_TEST(s1.find_first_not_of('a', 0) == 1);
        BOOST_TEST(s1.find_first_not_of('e', 4) == 5);
    }

    void
    testFindLastOf()
    {
        test_vectors const t;
        string const s1 = t.v1;

        // find_last_of(string_view, size_type)
        BOOST_TEST(s1.find_last_of("bcd") == 3);
        BOOST_TEST(s1.find_last_of("cde") == 4);

        BOOST_TEST(s1.find_last_of("bcd", 3) == 3);
        BOOST_TEST(s1.find_last_of("cde", 5) == 4);
        BOOST_TEST(s1.find_last_of("efg", 3) == string::npos);
    }

    void
    testFindLastNotOf()
    {
        test_vectors const t;
        string const s1 = t.v1;

        // find_last_not_of(string_view, size_type)
        BOOST_TEST(s1.find_last_not_of("abc", 3) == 3);
        BOOST_TEST(s1.find_last_not_of("bcd", 3) == 0);

        BOOST_TEST(s1.find_last_not_of("efg", 4) == 3);
        BOOST_TEST(s1.find_last_not_of("abc", 2) == string::npos);

        // find_first_not_of(char, size_type)
        BOOST_TEST(s1.find_last_not_of('a', 3) == 3);
        BOOST_TEST(s1.find_last_not_of('e', 4) == 3);
        BOOST_TEST(s1.find_last_not_of('a', 0) == string::npos);
    }

    void
    testNonMembers()
    {
        test_vectors const t;
        string const s1(t.v1);
        string const s2(t.v2);
        auto const v1(t.v1);
        auto const v2(t.v2);
        auto const c1 = t.s1.c_str();
        auto const c2 = t.s2.c_str();

        BOOST_TEST(! operator< (s1, s2));
        BOOST_TEST(! operator< (s1, v2));
        BOOST_TEST(! operator< (s1, c2));
        BOOST_TEST(! operator<=(s1, s2));
        BOOST_TEST(! operator<=(s1, v2));
        BOOST_TEST(! operator<=(s1, c2));
        BOOST_TEST(! operator==(s1, s2));
        BOOST_TEST(! operator==(s1, v2));
        BOOST_TEST(! operator==(s1, c2));
        BOOST_TEST(  operator!=(s1, s2));
        BOOST_TEST(  operator!=(s1, v2));
        BOOST_TEST(  operator!=(s1, c2));
        BOOST_TEST(  operator>=(s1, s2));
        BOOST_TEST(  operator>=(s1, v2));
        BOOST_TEST(  operator>=(s1, c2));
        BOOST_TEST(  operator> (s1, s2));
        BOOST_TEST(  operator> (s1, v2));
        BOOST_TEST(  operator> (s1, c2));

        BOOST_TEST(  operator< (s2, s1));
        BOOST_TEST(  operator< (s2, v1));
        BOOST_TEST(  operator< (s2, c1));
        BOOST_TEST(  operator<=(s2, s1));
        BOOST_TEST(  operator<=(s2, v1));
        BOOST_TEST(  operator<=(s2, c1));
        BOOST_TEST(  operator!=(s2, s1));
        BOOST_TEST(  operator!=(s2, v1));
        BOOST_TEST(  operator!=(s2, c1));
        BOOST_TEST(! operator==(s2, s1));
        BOOST_TEST(! operator==(s2, v1));
        BOOST_TEST(! operator==(s2, c1));
        BOOST_TEST(! operator>=(s2, s1));
        BOOST_TEST(! operator>=(s2, v1));
        BOOST_TEST(! operator>=(s2, c1));
        BOOST_TEST(! operator> (s2, s1));
        BOOST_TEST(! operator> (s2, v1));
        BOOST_TEST(! operator> (s2, c1));

        BOOST_TEST(  operator< (s2, s1));
        BOOST_TEST(  operator< (v2, s1));
        BOOST_TEST(  operator< (c2, s1));
        BOOST_TEST(  operator<=(s2, s1));
        BOOST_TEST(  operator<=(v2, s1));
        BOOST_TEST(  operator<=(c2, s1));
        BOOST_TEST(  operator!=(s2, s1));
        BOOST_TEST(  operator!=(v2, s1));
        BOOST_TEST(  operator!=(c2, s1));
        BOOST_TEST(! operator==(s2, s1));
        BOOST_TEST(! operator==(v2, s1));
        BOOST_TEST(! operator==(c2, s1));
        BOOST_TEST(! operator>=(s2, s1));
        BOOST_TEST(! operator>=(v2, s1));
        BOOST_TEST(! operator>=(c2, s1));
        BOOST_TEST(! operator> (s2, s1));
        BOOST_TEST(! operator> (v2, s1));
        BOOST_TEST(! operator> (c2, s1));

        BOOST_TEST(! operator< (s1, s2));
        BOOST_TEST(! operator< (v1, s2));
        BOOST_TEST(! operator< (c1, s2));
        BOOST_TEST(! operator<=(s1, s2));
        BOOST_TEST(! operator<=(v1, s2));
        BOOST_TEST(! operator<=(c1, s2));
        BOOST_TEST(! operator==(s1, s2));
        BOOST_TEST(! operator==(v1, s2));
        BOOST_TEST(! operator==(c1, s2));
        BOOST_TEST(  operator!=(s1, s2));
        BOOST_TEST(  operator!=(v1, s2));
        BOOST_TEST(  operator!=(c1, s2));
        BOOST_TEST(  operator>=(s1, s2));
        BOOST_TEST(  operator>=(v1, s2));
        BOOST_TEST(  operator>=(c1, s2));
        BOOST_TEST(  operator> (s1, s2));
        BOOST_TEST(  operator> (v1, s2));
        BOOST_TEST(  operator> (c1, s2));
    }

    void
    testHash()
    {
        // libstdc++ 4.8 bug
#if !defined(__GNUC__) || (__GNUC__ > 4 || \
    (__GNUC__ == 4 && __GNUC_MINOR__ > 8))
        {
            std::unordered_set<string> us;
            us.emplace("first");
            us.emplace("second");
        }
        {
            std::unordered_set<string>(
                0,
                std::hash<string>(32));
        }
#endif
        {
            std::hash<string> h1(32);
            std::hash<string> h2(h1);
            std::hash<string> h3(59);
            h1 = h3;
            h2 = h3;
            (void)h2;
        }
    }

    void
    run()
    {
        testConstruction();
        testAssignment();
        testAssign();
        testConversion();
        testElementAccess();
        testIterators();
        testCapacity();

        testClear();
        testInsert();
        testErase();
        testPushPop();
        testAppend();
        testPlusEquals();
        testCompare();
        testStartEndsWith();
        testReplace();
        testSubStr();
        testCopy();
        testResize();
        testSwap();

        testFind();
        testRfind();
        testFindFirstOf();
        testFindFirstNotOf();
        testFindLastOf();
        testFindLastNotOf();

        testNonMembers();

        testHash();
    }
};

TEST_SUITE(string_test, "boost.json.string");

BOOST_JSON_NS_END
