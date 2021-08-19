// Copyright (C) 2020 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Warning! This file is autogenerated.
#ifndef BOOST_TEXT_DATA_SI_HPP
#define BOOST_TEXT_DATA_SI_HPP

#include <boost/text/string_view.hpp>


namespace boost { namespace text { namespace data { namespace si {

inline string_view dictionary_collation_tailoring()
{
    return string_view((char const *)
u8R"(  
[normalization on]
[reorder Sinh Deva Beng Guru Gujr Orya Taml Telu Knda Mlym]
&ඖ<ං<ඃ
&ජ්ඤ<<ඥ
  )");
}

inline string_view standard_collation_tailoring()
{
    return string_view((char const *)
u8R"(  
[normalization on]
[reorder Sinh Deva Beng Guru Gujr Orya Taml Telu Knda Mlym]
&ඖ<ං<ඃ
&ඥ<ඤ
  )");
}


}}}}

#endif
