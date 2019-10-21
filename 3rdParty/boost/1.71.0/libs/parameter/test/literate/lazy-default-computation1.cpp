
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/parameter.hpp>
#include <string>
#include <functional>

BOOST_PARAMETER_NAME(s1)
BOOST_PARAMETER_NAME(s2)
BOOST_PARAMETER_NAME(s3)

template <typename ArgumentPack>
std::string f(ArgumentPack const& args)
{
    std::string const& s1 = args[_s1];
    std::string const& s2 = args[_s2];
    typename boost::parameter::binding<
        ArgumentPack, tag::s3, std::string
    >::type s3 = args[
        _s3 || boost::bind(
            std::plus<std::string>()
          , boost::ref(s1)
          , boost::ref(s2)
        )
    ];
    return s3;
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    std::string x = f((_s1="hello,", _s2=" world", _s3="hi world"));
    BOOST_TEST_EQ(x, std::string("hi world"));
    return boost::report_errors();
}

