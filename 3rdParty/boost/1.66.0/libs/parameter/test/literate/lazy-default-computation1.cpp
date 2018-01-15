
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/parameter.hpp>
#include <string>
#include <functional>

namespace parameter = boost::parameter;

BOOST_PARAMETER_NAME(s1)
BOOST_PARAMETER_NAME(s2)
BOOST_PARAMETER_NAME(s3)

template <class ArgumentPack>
std::string f(ArgumentPack const& args)
{
    std::string const& s1 = args[_s1];
    std::string const& s2 = args[_s2];
typename parameter::binding<
    ArgumentPack, tag::s3, std::string
>::type s3 = args[_s3
    || boost::bind(std::plus<std::string>(), boost::ref(s1), boost::ref(s2)) ];
    return s3;
}

std::string x = f((_s1="hello,", _s2=" world", _s3="hi world"));

int main()
{}

