
#include <boost/parameter.hpp>
#include <boost/mpl/is_sequence.hpp>
#include <boost/noncopyable.hpp>
#include <boost/type_traits/is_class.hpp>
#include <memory>

using namespace boost::parameter;

namespace boost { namespace python {

BOOST_PARAMETER_TEMPLATE_KEYWORD(class_type)
BOOST_PARAMETER_TEMPLATE_KEYWORD(base_list)
BOOST_PARAMETER_TEMPLATE_KEYWORD(held_type)
BOOST_PARAMETER_TEMPLATE_KEYWORD(copyable)

template <class B = int>
struct bases
{};

}}
namespace boost { namespace python {

using boost::mpl::_;

typedef parameter::parameters<
    required<tag::class_type, boost::is_class<_> >
  , parameter::optional<tag::base_list, mpl::is_sequence<_> >
  , parameter::optional<tag::held_type>
  , parameter::optional<tag::copyable>
> class_signature;

}}

namespace boost { namespace python {

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


using boost::python::class_type;
using boost::python::copyable;
using boost::python::held_type;
using boost::python::base_list;
using boost::python::bases;

struct B {};
struct D {};
typedef boost::python::class_<
    class_type<B>, copyable<boost::noncopyable>
> c1;

typedef boost::python::class_<
    D, held_type<std::auto_ptr<D> >, base_list<bases<B> >
> c2;

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
