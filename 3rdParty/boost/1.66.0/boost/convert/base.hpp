// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#ifndef BOOST_CONVERT_CONVERTER_BASE_HPP
#define BOOST_CONVERT_CONVERTER_BASE_HPP

#include <boost/convert/parameters.hpp>
#include <boost/convert/detail/is_string.hpp>
#include <cstring>

namespace boost { namespace cnv
{
    namespace ARG = boost::cnv::parameter;

    template<typename> struct cnvbase;
}}

#define BOOST_CNV_TO_STRING                                             \
    template<typename string_type>                                      \
    typename boost::enable_if<cnv::is_string<string_type>, void>::type  \
    operator()

#define BOOST_CNV_STRING_TO                                             \
    template<typename string_type>                                      \
    typename boost::enable_if<cnv::is_string<string_type>, void>::type  \
    operator()

#define BOOST_CNV_PARAM(param_name, param_type)                         \
    derived_type& operator()(boost::parameter::aux::tag<ARG::type::param_name, param_type>::type const& arg)

template<typename derived_type>
struct boost::cnv::cnvbase
{
    using   this_type = cnvbase;
    using    int_type = int;
    using   uint_type = unsigned int;
    using   lint_type = long int;
    using  ulint_type = unsigned long int;
    using   sint_type = short int;
    using  usint_type = unsigned short int;
    using  llint_type = long long int;
    using ullint_type = unsigned long long int;
    using    flt_type = float;
    using    dbl_type = double;
    using   ldbl_type = long double;

    // Integration of user-types via operator>>()
    template<typename type_in, typename type_out>
    void
    operator()(type_in const& in, boost::optional<type_out>& out) const
    {
        in >> out;
    }

    // Basic type to string
    BOOST_CNV_TO_STRING (   int_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING (  uint_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING (  lint_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING ( llint_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING ( ulint_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING (ullint_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING (  sint_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING ( usint_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING (   flt_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING (   dbl_type v, optional<string_type>& r) const { to_str_(v, r); }
    BOOST_CNV_TO_STRING (  ldbl_type v, optional<string_type>& r) const { to_str_(v, r); }
    // String to basic type
    BOOST_CNV_STRING_TO (string_type const& s, optional<   int_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional<  uint_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional<  lint_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional< llint_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional< ulint_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional<ullint_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional<  sint_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional< usint_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional<   flt_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional<   dbl_type>& r) const { str_to_(s, r); }
    BOOST_CNV_STRING_TO (string_type const& s, optional<  ldbl_type>& r) const { str_to_(s, r); }
    // Formatters
//  BOOST_CNV_PARAM (locale,  std::locale const) { locale_    = arg[ARG::   locale]; return dncast(); }
    BOOST_CNV_PARAM (base,     base::type const) { base_      = arg[ARG::     base]; return dncast(); }
    BOOST_CNV_PARAM (adjust, adjust::type const) { adjust_    = arg[ARG::   adjust]; return dncast(); }
    BOOST_CNV_PARAM (precision,       int const) { precision_ = arg[ARG::precision]; return dncast(); }
    BOOST_CNV_PARAM (precision,             int) { precision_ = arg[ARG::precision]; return dncast(); }
    BOOST_CNV_PARAM (uppercase,      bool const) { uppercase_ = arg[ARG::uppercase]; return dncast(); }
    BOOST_CNV_PARAM (skipws,         bool const) { skipws_    = arg[ARG::   skipws]; return dncast(); }
    BOOST_CNV_PARAM (width,           int const) { width_     = arg[ARG::    width]; return dncast(); }
    BOOST_CNV_PARAM (fill,           char const) {  fill_     = arg[ARG::     fill]; return dncast(); }

    protected:

    cnvbase()
    :
        base_       (10),
        skipws_     (false),
        precision_  (0),
        uppercase_  (false),
        width_      (0),
        fill_       (' '),
        adjust_     (boost::cnv::adjust::right)
    {}

    template<typename string_type, typename out_type>
    void
    str_to_(string_type const& str, optional<out_type>& result_out) const
    {
        cnv::range<string_type const> range (str);

        if (skipws_)
            for (; !range.empty() && cnv::is_space(*range.begin()); ++range);

        if (range.empty())                 return;
        if (cnv::is_space(*range.begin())) return;

        dncast().str_to(range, result_out);
    }
    template<typename in_type, typename string_type>
    void
    to_str_(in_type value_in, optional<string_type>& result_out) const
    {
        using  char_type = typename cnv::range<string_type>::value_type;
        using range_type = cnv::range<char_type*>;
        using   buf_type = char_type[bufsize_];

        buf_type     buf;
        range_type range = dncast().to_str(value_in, buf);
        char_type*   beg = range.begin();
        char_type*   end = range.end();
        int     str_size = end - beg;

        if (str_size <= 0)
            return;

        if (uppercase_)
            for (char_type* p = beg; p < end; ++p) *p = cnv::to_upper(*p);

        if (width_)
        {
            int  num_fill = (std::max)(0, int(width_ - (end - beg)));
            int  num_left = adjust_ == cnv::adjust::left ? 0
                          : adjust_ == cnv::adjust::right ? num_fill
                          : (num_fill / 2);
            int num_right = num_fill - num_left;
            bool     move = (beg < buf + num_left) // No room for left fillers
                         || (buf + bufsize_ < end + num_right); // No room for right fillers
            if (move)
            {
                std::memmove(buf + num_left, beg, str_size * sizeof(char_type));
                beg = buf + num_left;
                end = beg + str_size;
            }
            for (int k = 0; k <  num_left; *(--beg) = fill_, ++k);
            for (int k = 0; k < num_right; *(end++) = fill_, ++k);
        }
        result_out = string_type(beg, end);
    }

    derived_type const& dncast () const { return *static_cast<derived_type const*>(this); }
    derived_type&       dncast ()       { return *static_cast<derived_type*>(this); }

    // ULONG_MAX(8 bytes) = 18446744073709551615 (20(10) or 32(2) characters)
    // double (8 bytes) max is 316 chars
    static int const bufsize_ = 512;
    int                 base_;
    bool              skipws_;
    int            precision_;
    bool           uppercase_;
    int                width_;
    int                 fill_;
    adjust::type      adjust_;
//  std::locale       locale_;
};

#undef BOOST_CNV_TO_STRING
#undef BOOST_CNV_STRING_TO
#undef BOOST_CNV_PARAM

#endif // BOOST_CONVERT_CONVERTER_BASE_HPP
