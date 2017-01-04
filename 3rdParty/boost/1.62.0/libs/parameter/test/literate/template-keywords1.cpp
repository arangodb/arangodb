
#include <boost/parameter.hpp>
namespace boost { namespace python {

namespace tag { struct class_type; } // keyword tag type
template <class T>
struct class_type
  : parameter::template_keyword<tag::class_type,T>
{};

}}
