
#include <boost/parameter.hpp>

namespace test
{
  BOOST_PARAMETER_NAME(title)
  BOOST_PARAMETER_NAME(width)
  BOOST_PARAMETER_NAME(titlebar)

  BOOST_PARAMETER_FUNCTION(
     (int), new_window, tag, (required (title,*)(width,*)(titlebar,*)))
  {
     return 0;
  }

  BOOST_PARAMETER_TEMPLATE_KEYWORD(deleter)
  BOOST_PARAMETER_TEMPLATE_KEYWORD(copy_policy)

  template <class T> struct Deallocate {};
  struct DeepCopy {};

  namespace parameter = boost::parameter;

  struct Foo {};
  template <class T, class A0, class A1>
  struct smart_ptr
  {
      smart_ptr(Foo*);
  };
}
using namespace test;
int x = 
new_window("alert", _width=10, _titlebar=false);

smart_ptr<
   Foo
 , deleter<Deallocate<Foo> >
 , copy_policy<DeepCopy> > p(new Foo);
