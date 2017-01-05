
#include <boost/parameter.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/noncopyable.hpp>
#include <memory>

using namespace boost::parameter;
using boost::mpl::_;

namespace boost { namespace python {

BOOST_PARAMETER_TEMPLATE_KEYWORD(class_type)
BOOST_PARAMETER_TEMPLATE_KEYWORD(base_list)
BOOST_PARAMETER_TEMPLATE_KEYWORD(held_type)
BOOST_PARAMETER_TEMPLATE_KEYWORD(copyable)

}}
namespace boost { namespace python {

namespace detail { struct bases_base {}; }

template <class A0 = void, class A1 = void, class A2 = void  >
struct bases : detail::bases_base
{};

}}


#include <boost/type_traits/is_class.hpp>
namespace boost { namespace python {
typedef parameter::parameters<
    required<tag::class_type, is_class<_> >

  , parameter::optional<
        deduced<tag::base_list>
      , is_base_and_derived<detail::bases_base,_>
    >

  , parameter::optional<
        deduced<tag::held_type>
      , mpl::not_<
            mpl::or_<
                is_base_and_derived<detail::bases_base,_>
              , is_same<noncopyable,_>
            >
        >
    >

  , parameter::optional<deduced<tag::copyable>, is_same<noncopyable,_> >

> class_signature;
template <
    class A0
  , class A1 = parameter::void_
  , class A2 = parameter::void_
  , class A3 = parameter::void_
>
struct class_
{
    // Create ArgumentPack
    typedef typename
      class_signature::bind<A0,A1,A2,A3>::type
    args;

    // Extract first logical parameter.
    typedef typename parameter::value_type<
      args, tag::class_type>::type class_type;

    typedef typename parameter::value_type<
      args, tag::base_list, bases<> >::type base_list;

    typedef typename parameter::value_type<
      args, tag::held_type, class_type>::type held_type;

    typedef typename parameter::value_type<
      args, tag::copyable, void>::type copyable;
};

}}



struct B {};
struct D {};

using boost::python::bases;
typedef boost::python::class_<B, boost::noncopyable> c1;

typedef boost::python::class_<D, std::auto_ptr<D>, bases<B> > c2;
BOOST_MPL_ASSERT((boost::is_same<c1::class_type, B>));
BOOST_MPL_ASSERT((boost::is_same<c1::base_list, bases<> >));
BOOST_MPL_ASSERT((boost::is_same<c1::held_type, B>));
BOOST_MPL_ASSERT((
     boost::is_same<c1::copyable, boost::noncopyable>
));

BOOST_MPL_ASSERT((boost::is_same<c2::class_type, D>));
BOOST_MPL_ASSERT((boost::is_same<c2::base_list, bases<B> >));
BOOST_MPL_ASSERT((
    boost::is_same<c2::held_type, std::auto_ptr<D> >
));
BOOST_MPL_ASSERT((boost::is_same<c2::copyable, void>));

