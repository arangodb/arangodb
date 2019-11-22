
#include <boost/parameter.hpp>

namespace boost { namespace python {

    BOOST_PARAMETER_TEMPLATE_KEYWORD(class_type)
    BOOST_PARAMETER_TEMPLATE_KEYWORD(base_list)
    BOOST_PARAMETER_TEMPLATE_KEYWORD(held_type)
    BOOST_PARAMETER_TEMPLATE_KEYWORD(copyable)
}}

namespace boost { namespace python {
    namespace detail {

        struct bases_base
        {
        };
    }

    template <typename A0 = void, typename A1 = void, typename A2 = void>
    struct bases : detail::bases_base
    {
    };
}}

#include <boost/mpl/bool.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/eval_if.hpp>
#include <boost/noncopyable.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_base_of.hpp>
#include <boost/type_traits/is_class.hpp>
#include <boost/config.hpp>

#if !defined(BOOST_TT_HAS_CONFORMING_IS_CLASS_IMPLEMENTATION) || \
    !(1 == BOOST_TT_HAS_CONFORMING_IS_CLASS_IMPLEMENTATION)
#include <boost/type_traits/is_scalar.hpp>
#endif

namespace boost { namespace python {

    typedef boost::parameter::parameters<
        boost::parameter::required<
            tag::class_type
#if defined(BOOST_TT_HAS_CONFORMING_IS_CLASS_IMPLEMENTATION) && \
    (1 == BOOST_TT_HAS_CONFORMING_IS_CLASS_IMPLEMENTATION)
          , boost::mpl::if_<
                boost::is_class<boost::mpl::_>
              , boost::mpl::true_
              , boost::mpl::false_
            >
#else
          , boost::mpl::if_<
                boost::is_scalar<boost::mpl::_>
              , boost::mpl::false_
              , boost::mpl::true_
            >
#endif
        >
      , boost::parameter::optional<
            boost::parameter::deduced<tag::base_list>
          , boost::mpl::if_<
                boost::is_base_of<detail::bases_base,boost::mpl::_>
              , boost::mpl::true_
              , boost::mpl::false_
            >
        >
      , boost::parameter::optional<
            boost::parameter::deduced<tag::held_type>
          , boost::mpl::eval_if<
                boost::is_base_of<detail::bases_base,boost::mpl::_>
              , boost::mpl::false_
              , boost::mpl::if_<
                    boost::is_same<boost::noncopyable,boost::mpl::_>
                  , boost::mpl::false_
                  , boost::mpl::true_
                >
            >
        >
      , boost::parameter::optional<
            boost::parameter::deduced<tag::copyable>
          , boost::mpl::if_<
                boost::is_same<boost::noncopyable,boost::mpl::_>
              , boost::mpl::true_
              , boost::mpl::false_
            >
        >
    > class_signature;

    template <
        typename A0
      , typename A1 = boost::parameter::void_
      , typename A2 = boost::parameter::void_
      , typename A3 = boost::parameter::void_
    >
    struct class_
    {
        // Create ArgumentPack
        typedef typename boost::python::class_signature::BOOST_NESTED_TEMPLATE
        bind<A0,A1,A2,A3>::type args;

        // Extract first logical parameter.
        typedef typename boost::parameter::value_type<
            args,boost::python::tag::class_type
        >::type class_type;

        typedef typename boost::parameter::value_type<
            args,boost::python::tag::base_list,boost::python::bases<>
        >::type base_list;

        typedef typename boost::parameter::value_type<
            args,boost::python::tag::held_type,class_type
        >::type held_type;

        typedef typename boost::parameter::value_type<
            args,boost::python::tag::copyable,void
        >::type copyable;
    };
}}

struct B
{
};

struct D
{
};

typedef boost::python::class_<B,boost::noncopyable> c1;

#include <memory>

#if defined(BOOST_NO_CXX11_SMART_PTR)
typedef boost::python::class_<D,std::auto_ptr<D>,boost::python::bases<B> > c2;
#else
typedef boost::python::class_<
    D,std::unique_ptr<D>,boost::python::bases<B>
> c2;
#endif

#include <boost/mpl/assert.hpp>
#include <boost/mpl/aux_/test.hpp>

MPL_TEST_CASE()
{
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c1::class_type,B>
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c1::base_list,boost::python::bases<> >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c1::held_type,B>
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c1::copyable,boost::noncopyable>
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c2::class_type,D>
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c2::base_list,boost::python::bases<B> >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
#if defined(BOOST_NO_CXX11_SMART_PTR)
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c2::held_type,std::auto_ptr<D> >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
#else
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c2::held_type,std::unique_ptr<D> >
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
#endif  // BOOST_NO_CXX11_SMART_PTR
    BOOST_MPL_ASSERT((
        boost::mpl::if_<
            boost::is_same<c2::copyable,void>
          , boost::mpl::true_
          , boost::mpl::false_
        >::type
    ));
}

