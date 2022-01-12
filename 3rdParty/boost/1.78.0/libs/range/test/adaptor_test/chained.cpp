// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//
// For more information, see http://www.boost.org/libs/range/
//
// Credits:
//  Jurgen Hunold provided a test case that demonstrated that the range adaptors
//  were producing iterators that were not default constructible. This became
//  symptomatic after enabling concept checking assertions. This test is a
//  lightly modified version of his supplied code to ensure that his use case
//  never breaks again. (hopefully!)
//

#include <boost/range/adaptor/transformed.hpp>
#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/bind/bind.hpp>

#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>

#include <vector>
#include <set>

namespace boost_range_test
{
    namespace
    {

class foo
{
public:
    static foo from_string(const std::string& source)
    {
        foo f;
        f.m_valid = true;
        f.m_value = 0u;
        for (std::string::const_iterator it = source.begin();
             it != source.end(); ++it)
        {
            f.m_value += *it;
            if ((*it < 'a') || (*it > 'z'))
                f.m_valid = false;
        }
        return f;
    }
    bool is_valid() const
    {
        return m_valid;
    }
    bool operator<(const foo& other) const
    {
        return m_value < other.m_value;
    }
    bool operator==(const foo& other) const
    {
        return m_value == other.m_value && m_valid == other.m_valid;
    }
    bool operator!=(const foo& other) const
    {
        return !operator==(other);
    }

    friend inline std::ostream& operator<<(std::ostream& out, const foo& obj)
    {
        out << "{value=" << obj.m_value
            << ", valid=" << std::boolalpha << obj.m_valid << "}\n";
        return out;
    }

private:
    boost::uint64_t m_value;
    bool m_valid;
};

void chained_adaptors_test()
{
    std::vector<std::string> sep;

    sep.push_back("AB");
    sep.push_back("ab");
    sep.push_back("aghj");

    std::set<foo> foos;

    using namespace boost::placeholders;

    boost::copy(sep
        | boost::adaptors::transformed(boost::bind(&foo::from_string, _1))
        | boost::adaptors::filtered(boost::bind(&foo::is_valid, _1)),
        std::inserter(foos, foos.end()));

    std::vector<foo> reference;
    reference.push_back(foo::from_string("ab"));
    reference.push_back(foo::from_string("aghj"));

    BOOST_CHECK_EQUAL_COLLECTIONS(
                reference.begin(), reference.end(),
                foos.begin(), foos.end());
}

    } // anonymous namespace
} // namespace boost_range_test

boost::unit_test::test_suite*
init_unit_test_suite(int argc, char* argv[])
{
    boost::unit_test::test_suite* test
        = BOOST_TEST_SUITE( "RangeTestSuite.adaptor.chained adaptors" );

    test->add(BOOST_TEST_CASE( boost_range_test::chained_adaptors_test));

    return test;
}
