// Copyright Cromwell D. Enage 2018.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/config.hpp>

#if !defined(BOOST_PARAMETER_HAS_PERFECT_FORWARDING) && \
    (BOOST_PARAMETER_COMPOSE_MAX_ARITY < 3)
#error Define BOOST_PARAMETER_COMPOSE_MAX_ARITY as 3 or greater.
#endif

#include <boost/parameter/name.hpp>

namespace test {

    BOOST_PARAMETER_NAME(a0)
    BOOST_PARAMETER_NAME(a1)
    BOOST_PARAMETER_NAME(a2)
}

#if !defined(BOOST_NO_SFINAE)
#include <boost/parameter/is_argument_pack.hpp>
#include <boost/type_traits/is_convertible.hpp>
#include <boost/core/enable_if.hpp>
#endif

namespace test {

#if !defined(BOOST_NO_SFINAE)
    struct _enabler
    {
    };
#endif

    template <typename T>
    class backend0
    {
        T _a0;

     public:
        template <typename ArgPack>
        explicit backend0(
            ArgPack const& args
#if !defined(BOOST_NO_SFINAE)
          , typename boost::enable_if<
                boost::parameter::is_argument_pack<ArgPack>
              , test::_enabler
            >::type = test::_enabler()
#endif
        ) : _a0(args[test::_a0])
        {
        }

#if !defined(BOOST_NO_SFINAE)
        template <typename U>
        backend0(
            backend0<U> const& copy
          , typename boost::enable_if<
                boost::is_convertible<U,T>
              , test::_enabler
            >::type = test::_enabler()
        ) : _a0(copy.get_a0())
        {
        }
#endif

        template <typename Iterator>
        backend0(Iterator itr, Iterator itr_end) : _a0(itr, itr_end)
        {
        }

        T const& get_a0() const
        {
            return this->_a0;
        }

     protected:
        template <typename ArgPack>
        void initialize_impl(ArgPack const& args)
        {
            this->_a0 = args[test::_a0];
        }
    };

    template <typename B, typename T>
    class backend1 : public B
    {
        T _a1;

     public:
        template <typename ArgPack>
        explicit backend1(
            ArgPack const& args
#if !defined(BOOST_NO_SFINAE)
          , typename boost::enable_if<
                boost::parameter::is_argument_pack<ArgPack>
              , test::_enabler
            >::type = test::_enabler()
#endif
        ) : B(args), _a1(args[test::_a1])
        {
        }

#if !defined(BOOST_NO_SFINAE)
        template <typename Derived>
        backend1(
            Derived const& copy
          , typename boost::disable_if<
                boost::parameter::is_argument_pack<Derived>
              , test::_enabler
            >::type = test::_enabler()
        ) : B(copy), _a1(copy.get_a1())
        {
        }
#endif

        T const& get_a1() const
        {
            return this->_a1;
        }

     protected:
        template <typename ArgPack>
        void initialize_impl(ArgPack const& args)
        {
            B::initialize_impl(args);
            this->_a1 = args[test::_a1];
        }
    };

    template <typename B, typename T>
    class backend2 : public B
    {
        T _a2;

     public:
        template <typename ArgPack>
        explicit backend2(
            ArgPack const& args
#if !defined(BOOST_NO_SFINAE)
          , typename boost::enable_if<
                boost::parameter::is_argument_pack<ArgPack>
              , test::_enabler
            >::type = test::_enabler()
#endif
        ) : B(args), _a2(args[test::_a2])
        {
        }

#if !defined(BOOST_NO_SFINAE)
        template <typename Derived>
        backend2(
            Derived const& copy
          , typename boost::disable_if<
                boost::parameter::is_argument_pack<Derived>
              , test::_enabler
            >::type = test::_enabler()
        ) : B(copy), _a2(copy.get_a2())
        {
        }
#endif

        T const& get_a2() const
        {
            return this->_a2;
        }

     protected:
        template <typename ArgPack>
        void initialize_impl(ArgPack const& args)
        {
            B::initialize_impl(args);
            this->_a2 = args[test::_a2];
        }
    };
}

#include <boost/parameter/preprocessor_no_spec.hpp>

#if !defined(BOOST_NO_SFINAE)
#include <boost/parameter/are_tagged_arguments.hpp>
#endif

namespace test {

    template <typename B>
    struct frontend : public B
    {
        BOOST_PARAMETER_NO_SPEC_CONSTRUCTOR(frontend, (B))

#if !defined(BOOST_NO_SFINAE)
        template <typename Iterator>
        frontend(
            Iterator itr
          , Iterator itr_end
          , typename boost::disable_if<
                boost::parameter::are_tagged_arguments<Iterator>
              , test::_enabler
            >::type = test::_enabler()
        ) : B(itr, itr_end)
        {
        }

        template <typename O>
        frontend(frontend<O> const& copy) : B(copy)
        {
        }
#endif

        BOOST_PARAMETER_NO_SPEC_MEMBER_FUNCTION((void), initialize)
        {
            this->initialize_impl(args);
        }

        BOOST_PARAMETER_NO_SPEC_FUNCTION_CALL_OPERATOR((void))
        {
            this->initialize_impl(args);
        }
    };
} // namespace test

#include <boost/core/lightweight_test.hpp>

#if !defined(BOOST_NO_SFINAE)
#include <string>
#endif

int main()
{
    char const* p = "foo";
    char const* q = "bar";
    test::frontend<
        test::backend2<test::backend1<test::backend0<char const*>, char>, int>
    > composed_obj0(test::_a2 = 4, test::_a1 = ' ', test::_a0 = p);
#if defined(BOOST_NO_SFINAE)
    test::frontend<
        test::backend1<test::backend2<test::backend0<char const*>, int>, char>
    > composed_obj1(test::_a0 = p, test::_a1 = ' ', test::_a2 = 4);
#else
    test::frontend<
        test::backend1<test::backend2<test::backend0<char const*>, int>, char>
    > composed_obj1(composed_obj0);
#endif
    BOOST_TEST_EQ(composed_obj0.get_a0(), composed_obj1.get_a0());
    BOOST_TEST_EQ(composed_obj0.get_a1(), composed_obj1.get_a1());
    BOOST_TEST_EQ(composed_obj0.get_a2(), composed_obj1.get_a2());
    composed_obj0.initialize(test::_a0 = q, test::_a1 = '!', test::_a2 = 8);
    composed_obj1.initialize(test::_a2 = 8, test::_a1 = '!', test::_a0 = q);
    BOOST_TEST_EQ(composed_obj0.get_a0(), composed_obj1.get_a0());
    BOOST_TEST_EQ(composed_obj0.get_a1(), composed_obj1.get_a1());
    BOOST_TEST_EQ(composed_obj0.get_a2(), composed_obj1.get_a2());
    composed_obj0(test::_a2 = 8, test::_a1 = '!', test::_a0 = q);
    composed_obj1(test::_a0 = q, test::_a1 = '!', test::_a2 = 8);
    BOOST_TEST_EQ(composed_obj0.get_a0(), composed_obj1.get_a0());
    BOOST_TEST_EQ(composed_obj0.get_a1(), composed_obj1.get_a1());
    BOOST_TEST_EQ(composed_obj0.get_a2(), composed_obj1.get_a2());
#if !defined(BOOST_NO_SFINAE)
    test::frontend<test::backend0<std::string> > string_wrap(p, p + 3);
    BOOST_TEST_EQ(string_wrap.get_a0(), std::string(p));
#endif
    return boost::report_errors();
}

