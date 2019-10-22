/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Andrey Semashev 2018.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#include <boost/intrusive/options.hpp>
#include <boost/intrusive/set.hpp>
#include <boost/intrusive/set_hook.hpp>
#include <boost/config.hpp>
#include <boost/core/lightweight_test.hpp>
#include <functional> // std::less

// The test verifies that the set implementation does not use void* as auxiliary arguments for SFINAE
// in internal functions, which would make overload resolution ambiguous if user's key type is also void*.

typedef boost::intrusive::set_base_hook<
    boost::intrusive::link_mode< boost::intrusive::safe_link >,
    boost::intrusive::tag< struct for_set_element_lookup_by_key >,
    boost::intrusive::optimize_size< true >
> set_element_hook_t;

struct set_element :
    public set_element_hook_t
{
    struct order_by_key
    {
        typedef bool result_type;

        result_type operator() (set_element const& left, set_element const& right) const
        {
            return std::less< void* >()(left.m_key, right.m_key);
        }
        result_type operator() (void* left, set_element const& right) const
        {
            return std::less< void* >()(left, right.m_key);
        }
        result_type operator() (set_element const& left, void* right) const
        {
            return std::less< void* >()(left.m_key, right);
        }
    };

    void* m_key;

    explicit set_element(void* key) : m_key(key) {}

    BOOST_DELETED_FUNCTION(set_element(set_element const&))
    BOOST_DELETED_FUNCTION(set_element& operator=(set_element const&))
};

typedef boost::intrusive::set<
    set_element,
    boost::intrusive::base_hook< set_element_hook_t >,
    boost::intrusive::compare< set_element::order_by_key >,
    boost::intrusive::constant_time_size< true >
> set_t;

void test_set()
{
    int v1 = 0, v2 = 1, v3 = 2;
    set_element e1(&v1), e2(&v2), e3(&v3);

    set_t s;
    s.insert(e1);
    s.insert(e2);

    set_t::iterator it = s.find(e1);
    BOOST_TEST(it != s.end() && &*it == &e1);

    it = s.find((void*)&v2, set_element::order_by_key());
    BOOST_TEST(it != s.end() && &*it == &e2);

    it = s.find(e3);
    BOOST_TEST(it == s.end());

    it = s.find((void*)&v3, set_element::order_by_key());
    BOOST_TEST(it == s.end());

    s.clear();
}

int main()
{
    test_set();

    return boost::report_errors();
}
