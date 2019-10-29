
#include <boost/parameter.hpp>

namespace boost { namespace python {

    BOOST_PARAMETER_TEMPLATE_KEYWORD(class_type)
    BOOST_PARAMETER_TEMPLATE_KEYWORD(base_list)
    BOOST_PARAMETER_TEMPLATE_KEYWORD(held_type)
    BOOST_PARAMETER_TEMPLATE_KEYWORD(copyable)

    template <typename B = int>
    struct bases
    {
    };
}}

#include <boost/mpl/bool.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/if.hpp>
#include <boost/mpl/is_sequence.hpp>
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
            tag::base_list
          , boost::mpl::is_sequence<boost::mpl::_>
        >
      , boost::parameter::optional<tag::held_type>
      , boost::parameter::optional<tag::copyable>
    > class_signature;
}} // namespace boost::python

namespace boost { namespace python {

    template <
        typename A0
      , typename A1 = boost::parameter::void_
      , typename A2 = boost::parameter::void_
      , typename A3 = boost::parameter::void_
    >
    struct class_
    {
        // Create ArgumentPack
        typedef typename class_signature::BOOST_NESTED_TEMPLATE bind<
            A0, A1, A2, A3
        >::type args;

        // Extract first logical parameter.
        typedef typename boost::parameter::value_type<
            args, tag::class_type
        >::type class_type;

        typedef typename boost::parameter::value_type<
            args, tag::base_list, boost::python::bases<>
        >::type base_list;

        typedef typename boost::parameter::value_type<
            args, tag::held_type, class_type
        >::type held_type;

        typedef typename boost::parameter::value_type<
            args, tag::copyable, void
        >::type copyable;
    };
}} // namespace boost::python

struct B
{
};

struct D
{
};

#include <boost/noncopyable.hpp>
#include <memory>

typedef boost::python::class_<
    boost::python::class_type<B>
  , boost::python::copyable<boost::noncopyable>
> c1;

typedef boost::python::class_<
    D
  , boost::python::held_type<
#if defined(BOOST_NO_CXX11_SMART_PTR)
        std::auto_ptr<D>
#else
        std::unique_ptr<D>
#endif
    >
  , boost::python::base_list<boost::python::bases<B> >
> c2;

#include <boost/type_traits/is_same.hpp>
#include <boost/mpl/aux_/test.hpp>
#include <boost/mpl/assert.hpp>

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

