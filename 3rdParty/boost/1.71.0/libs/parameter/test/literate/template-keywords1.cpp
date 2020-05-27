
#include <boost/parameter.hpp>

namespace boost { namespace python {

    namespace tag {

        struct class_type;  // keyword tag type
    }

    template <typename T>
    struct class_type
      : boost::parameter::template_keyword<boost::python::tag::class_type,T>
    {
    };
}}

