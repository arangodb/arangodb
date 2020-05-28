
#include <boost/parameter.hpp>

namespace test {

    BOOST_PARAMETER_NAME(title)
    BOOST_PARAMETER_NAME(width)
    BOOST_PARAMETER_NAME(titlebar)

    BOOST_PARAMETER_FUNCTION((int), new_window, tag,
        (required (title,*)(width,*)(titlebar,*))
    )
    {
        return 0;
    }

    BOOST_PARAMETER_TEMPLATE_KEYWORD(deleter)
    BOOST_PARAMETER_TEMPLATE_KEYWORD(copy_policy)

    template <typename T>
    struct Deallocate
    {
    };

    struct DeepCopy
    {
    };

    struct Foo
    {
    };

    template <typename T, typename A0, typename A1>
    struct smart_ptr
    {
        smart_ptr(test::Foo*)
        {
        }
    };
}

#include <boost/core/lightweight_test.hpp>

int main()
{
    char const* alert_s = "alert";
    int x = test::new_window(alert_s, test::_width=10, test::_titlebar=false);
    test::Foo* foo = new test::Foo();
    test::smart_ptr<
        test::Foo
      , test::deleter<test::Deallocate<test::Foo> >
      , test::copy_policy<test::DeepCopy>
    > p(foo);
    delete foo;
    return boost::report_errors();
}

