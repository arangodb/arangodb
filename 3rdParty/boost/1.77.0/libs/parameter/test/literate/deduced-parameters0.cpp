
#include <boost/parameter.hpp>

BOOST_PARAMETER_NAME(name)
BOOST_PARAMETER_NAME(func)
BOOST_PARAMETER_NAME(docstring)
BOOST_PARAMETER_NAME(keywords)
BOOST_PARAMETER_NAME(policies)

struct default_call_policies
{
};

struct no_keywords
{
};

struct keywords
{
};

#include <boost/mpl/bool.hpp>

template <typename T>
struct is_keyword_expression
  : boost::mpl::false_
{
};

template <>
struct is_keyword_expression<keywords>
  : boost::mpl::true_
{
};

default_call_policies some_policies;

void f()
{
}

#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/type_traits/is_convertible.hpp>

char const*& blank_char_ptr()
{
    static char const* larr = "";
    return larr;
}

BOOST_PARAMETER_FUNCTION(
    (bool), def, tag,
    (required (name,(char const*)) (func,*) )  // nondeduced
    (deduced
        (optional
            (docstring, (char const*), blank_char_ptr())
            (keywords
                // see 5
              , *(is_keyword_expression<boost::mpl::_>)
              , no_keywords()
            )
            (policies
              , *(
                    boost::mpl::eval_if<
                        boost::is_convertible<boost::mpl::_,char const*>
                      , boost::mpl::false_
                      , boost::mpl::if_<
                            // see 5
                            is_keyword_expression<boost::mpl::_>
                          , boost::mpl::false_
                          , boost::mpl::true_
                        >
                    >
                )
              , default_call_policies()
            )
        )
    )
)
{
    return true;
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    char const* f_name = "f";
    def(f_name, &f, some_policies, "Documentation for f");
    def(f_name, &f, "Documentation for f", some_policies);
    def(
        f_name
      , &f
      , _policies = some_policies
      , "Documentation for f"
    );
    return boost::report_errors();
}

