// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>
#include <boost/process/environment.hpp>

namespace bp = boost::process;


namespace std
{
std::ostream & operator<<(std::ostream & str, const std::wstring & ws)
{
    str << bp::detail::convert(ws);
    return str;
}
}

BOOST_AUTO_TEST_CASE(empty,  *boost::unit_test::timeout(5))
{
    bp::environment ev ;
    BOOST_CHECK(ev.empty());
    BOOST_CHECK_EQUAL(ev.size(), 0u);
    BOOST_CHECK_EQUAL(ev.end() - ev.begin(), 0);
    ev["Thingy"] = "My value";

    BOOST_CHECK(!ev.empty());
    BOOST_CHECK_EQUAL(ev.size(), 1u);
    BOOST_CHECK_EQUAL(ev.end() - ev.begin(), 1);

    for (auto  x : ev)
    {
        BOOST_CHECK_EQUAL(x.to_string(), "My value");
        BOOST_CHECK_EQUAL(x.get_name(), "Thingy");
    }

    ev["Thingy"].clear();
    BOOST_CHECK(ev.empty());
    BOOST_CHECK_EQUAL(ev.size(), 0u);
    BOOST_CHECK_EQUAL(ev.end() - ev.begin(), 0);
    ev.clear();
}

BOOST_AUTO_TEST_CASE(wempty,  *boost::unit_test::timeout(5))
{
    bp::wenvironment ev ;
    BOOST_CHECK(ev.empty());
    BOOST_CHECK_EQUAL(ev.size(), 0u);
    BOOST_CHECK_EQUAL(ev.end() - ev.begin(), 0);
    ev[L"Thingy"] = L"My value";

    BOOST_CHECK(!ev.empty());
    BOOST_CHECK_EQUAL(ev.size(), 1u);
    BOOST_CHECK_EQUAL(ev.end() - ev.begin(), 1);

    for (auto  x : ev)
    {
        BOOST_CHECK(x.to_string() == L"My value");
        BOOST_CHECK(x.get_name()  == L"Thingy");
    }

    ev[L"Thingy"].clear();
    BOOST_CHECK(ev.empty());
    BOOST_CHECK_EQUAL(ev.size(), 0u);
    BOOST_CHECK_EQUAL(ev.end() - ev.begin(), 0);
    ev.clear();
}

BOOST_AUTO_TEST_CASE(compare,  *boost::unit_test::timeout(5))
{
    auto nat = boost::this_process::environment();
    bp::environment env = nat;

    {
        BOOST_CHECK_EQUAL(nat.size(), env.size());
        auto ni = nat.begin();
        auto ei = env.begin();

        while ((ni != nat.end()) &&(ei != env.end()))
        {
            BOOST_CHECK_EQUAL(ni->get_name(),  ei->get_name());
            BOOST_CHECK_EQUAL(ni->to_string(), ei->to_string());
            ni++; ei++;
        }
    }

    //ok check if I can convert it.
    bp::wenvironment wenv{env};
    auto wnat = boost::this_process::wenvironment();
    BOOST_CHECK_EQUAL(wenv.size(), env.size());
    BOOST_CHECK_EQUAL(wnat.size(), nat.size());
    {
        BOOST_CHECK_EQUAL(wnat.size(), wenv.size());
        auto ni = wnat.begin();
        auto ei = wenv.begin();

        while ((ni != wnat.end()) && (ei != wenv.end()))
        {
            BOOST_CHECK_EQUAL(ni->get_name() , ei->get_name());
            BOOST_CHECK_EQUAL(ni->to_string(), ei->to_string());
            ni++; ei++;
        }

        BOOST_CHECK(ni == wnat.end());
    }
    BOOST_TEST_PASSPOINT();
    env.clear();
    BOOST_TEST_PASSPOINT();
    wenv.clear();
    BOOST_TEST_PASSPOINT();
}

BOOST_AUTO_TEST_CASE(wcompare,  *boost::unit_test::timeout(5))
{
    auto nat = boost::this_process::wenvironment();
    bp::wenvironment env = nat;

    {
        BOOST_CHECK_EQUAL(nat.size(), env.size());
        auto ni = nat.begin();
        auto ei = env.begin();

        while ((ni != nat.end()) &&(ei != env.end()))
        {
            BOOST_CHECK_EQUAL(ni->get_name(),  ei->get_name());
            BOOST_CHECK_EQUAL(ni->to_string(), ei->to_string());
            ni++; ei++;
        }
    }

    BOOST_TEST_PASSPOINT();
    env.clear();
    BOOST_TEST_PASSPOINT();
}

BOOST_AUTO_TEST_CASE(insert_remove,  *boost::unit_test::timeout(5))
{
    bp::environment env(boost::this_process::environment());

    auto sz = env.size();
    BOOST_REQUIRE_GE(sz, 1u);
    BOOST_REQUIRE_EQUAL(env.count("BOOST_TEST_VAR"), 0u);

    env["BOOST_TEST_VAR"] = {"some string", "badabumm"};

#if defined(BOOST_WINDOWS_API)
    BOOST_CHECK_EQUAL(env["BOOST_TEST_VAR"].to_string(), "some string;badabumm");
#else
    BOOST_CHECK_EQUAL(env["BOOST_TEST_VAR"].to_string(), "some string:badabumm");
#endif
    BOOST_CHECK_EQUAL(sz +1, env.size());

    env["BOOST_TEST_VAR"].clear();

    BOOST_CHECK_EQUAL(env.size(), sz);

    env.clear();

}

BOOST_AUTO_TEST_CASE(clear_empty_my,  *boost::unit_test::timeout(5))
{
    bp::native_environment env;

    bp::environment e = env;

    const std::size_t sz = env.size();

    BOOST_TEST_MESSAGE("Current native size: " << sz);

    BOOST_REQUIRE_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_a"), 0u);
    BOOST_REQUIRE_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_b"), 0u);
    BOOST_REQUIRE_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_c"), 0u);

    env["BOOST_PROCESS_TEST_VAR_a"] = "1";
    env["BOOST_PROCESS_TEST_VAR_b"] = "2";
    BOOST_CHECK(env.emplace("BOOST_PROCESS_TEST_VAR_c", "3").second);

    BOOST_CHECK_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_a"), 1u);
    BOOST_CHECK_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_b"), 1u);
    BOOST_CHECK_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_c"), 1u);

    BOOST_CHECK_EQUAL(env.at("BOOST_PROCESS_TEST_VAR_a").to_string(), "1");
    BOOST_CHECK_EQUAL(env.at("BOOST_PROCESS_TEST_VAR_b").to_string(), "2");
    BOOST_CHECK_EQUAL(env.at("BOOST_PROCESS_TEST_VAR_c").to_string(), "3");
    BOOST_CHECK_EQUAL(env.size(), sz + 3u);
    BOOST_CHECK_EQUAL(std::distance(env.begin(),  env.end()),  sz + 3);
    BOOST_CHECK_EQUAL(std::distance(env.cbegin(), env.cend()), sz + 3);

    env.erase("BOOST_PROCESS_TEST_VAR_a");
    BOOST_CHECK_EQUAL(env.size(), sz + 2u);
    BOOST_CHECK_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_a"), 0u);
    BOOST_CHECK_EQUAL(env.at   ("BOOST_PROCESS_TEST_VAR_b").to_string(), "2");
    BOOST_CHECK_EQUAL(env.at   ("BOOST_PROCESS_TEST_VAR_c").to_string(), "3");

    BOOST_CHECK_EQUAL(std::distance(env.begin(),  env.end()),  sz + 2);
    BOOST_CHECK_EQUAL(std::distance(env.cbegin(), env.cend()), sz + 2);

    env.erase("BOOST_PROCESS_TEST_VAR_b");
    BOOST_CHECK_EQUAL(env.size(), sz + 1u);
    BOOST_CHECK_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_a"), 0u);
    BOOST_CHECK_EQUAL(env.count("BOOST_PROCESS_TEST_VAR_b"), 0u);
    BOOST_CHECK_EQUAL(env.at   ("BOOST_PROCESS_TEST_VAR_c").to_string(), "3");

    BOOST_CHECK_EQUAL(std::distance(env.begin(),  env.end()),  sz + 1);
    BOOST_CHECK_EQUAL(std::distance(env.cbegin(), env.cend()), sz + 1);

    env.clear();
    //note: windows puts an entry without a name into the list, so it might not be empty after clear.
    BOOST_CHECK_LE(env.size(), sz);

    BOOST_CHECK_LE(std::distance(env.begin(),  env.end()),  sz);
    BOOST_CHECK_LE(std::distance(env.cbegin(), env.cend()), sz);

    for (auto && ee : e)
        env.emplace(ee.get_name(), ee.to_string());
}

BOOST_AUTO_TEST_CASE(clear_empty,  *boost::unit_test::timeout(5))
{
    bp::environment env;
    BOOST_CHECK(env.empty());
    BOOST_CHECK_EQUAL(env.size(), 0u);
    env["a"] = "1";
    env["b"] = "2";
    env["c"] = "3";

    BOOST_CHECK_EQUAL(env.at("a").to_string(), "1");
    BOOST_CHECK_EQUAL(env.at("b").to_string(), "2");
    BOOST_CHECK_EQUAL(env.at("c").to_string(), "3");
    BOOST_CHECK_EQUAL(env.size(), 3u);
    BOOST_CHECK_EQUAL(std::distance(env.begin(), env.end()), 3u);
    BOOST_CHECK_EQUAL(std::distance(env.cbegin(), env.cend()), 3u);

    env.erase("c");
    BOOST_CHECK_EQUAL(env.size(), 2u);
    BOOST_CHECK_EQUAL(env.at("a").to_string(), "1");
    BOOST_CHECK_EQUAL(env.at("b").to_string(), "2");
    BOOST_CHECK_EQUAL(env.count("c"), 0u);

    BOOST_CHECK_EQUAL(std::distance(env.begin(), env.end()), 2u);
    BOOST_CHECK_EQUAL(std::distance(env.cbegin(), env.cend()), 2u);

    env.erase("b");
    BOOST_CHECK_EQUAL(env.size(), 1u);
    BOOST_CHECK_EQUAL(env.at("a").to_string(), "1");
    BOOST_CHECK_EQUAL(env.count("b"), 0u);
    BOOST_CHECK_EQUAL(env.count("c"), 0u);

    BOOST_CHECK_EQUAL(std::distance(env.begin(), env.end()), 1u);
    BOOST_CHECK_EQUAL(std::distance(env.cbegin(), env.cend()), 1u);

    env.clear();
    BOOST_CHECK(env.empty());
    BOOST_CHECK_EQUAL(env.size(), 0u);

    BOOST_CHECK_EQUAL(std::distance(env.begin(), env.end()), 0u);
    BOOST_CHECK_EQUAL(std::distance(env.cbegin(), env.cend()), 0u);

}
