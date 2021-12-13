/*
Copyright 2002, 2003 Daryle Walker

Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/io/ios_state.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <iomanip>
#include <ios>
#include <istream>
#include <locale>
#include <ostream>
#include <sstream>
#if defined(BOOST_GCC) || (defined(BOOST_CLANG) && defined(BOOST_GNU_STDLIB))
#include <stdexcept>
#endif
#include <streambuf>
#include <string>
#include <cstddef>

class backward_bool_names
    : public std::numpunct<char> {
    typedef std::numpunct<char> base_type;

public:
    explicit backward_bool_names(std::size_t refs = 0)
        : base_type( refs ) { }

protected:
    ~backward_bool_names() { }

    base_type::string_type do_truename() const {
        return "eurt";
    }

    base_type::string_type do_falsename() const {
        return "eslaf";
    }
};

void saver_tests_1(int index,
    std::istream& input,
    std::ostream& output,
    std::ostream& err)
{
    boost::io::ios_flags_saver     ifls(output);
    boost::io::ios_precision_saver iprs(output);
    boost::io::ios_width_saver     iws(output);
    boost::io::ios_tie_saver       its(input);
    boost::io::ios_rdbuf_saver     irs(output);
    boost::io::ios_fill_saver      ifis(output);
    boost::io::ios_locale_saver    ils(output);
    boost::io::ios_iword_saver     iis(output, index);
    boost::io::ios_pword_saver     ipws(output, index);
    std::locale loc(std::locale::classic(), new backward_bool_names);
    input.tie(&err);
    output.rdbuf(err.rdbuf());
    output.iword(index) = 69L;
    output.pword(index) = &err;
    output.setf(std::ios_base::showpos | std::ios_base::boolalpha);
    output.setf(std::ios_base::internal, std::ios_base::adjustfield);
    output.fill('@');
    output.precision( 9 );
    output << "Hello world";
    output << std::setw(10) << -16;
    output << std::setw(15) << 34.5678901234;
    output.imbue(loc);
    output << true;
    BOOST_TEST(&err == output.pword(index));
    BOOST_TEST(69L == output.iword(index));
    try {
        boost::io::ios_exception_saver ies(output);
        boost::io::ios_iostate_saver ias(output);
        output.exceptions(std::ios_base::eofbit | std::ios_base::badbit);
        output.setstate(std::ios_base::eofbit);
        BOOST_ERROR("previous line should have thrown");
#if defined(BOOST_GCC) || (defined(BOOST_CLANG) && defined(BOOST_GNU_STDLIB))
    } catch (std::exception&) {
#else
    } catch (std::ios_base::failure&) {
#endif
        BOOST_TEST(output.exceptions() == std::ios_base::goodbit );
    }
}

void saver_tests_2(int index,
    std::istream& input,
    std::ostream& output,
    std::ostream& err)
{
    boost::io::ios_tie_saver   its(input, &err);
    boost::io::ios_rdbuf_saver irs(output, err.rdbuf());
    boost::io::ios_iword_saver iis(output, index, 69L);
    boost::io::ios_pword_saver ipws(output, index, &err);
    boost::io::ios_flags_saver ifls(output,
        (output.flags() & ~std::ios_base::adjustfield) |
        std::ios_base::showpos |
        std::ios_base::boolalpha |
        (std::ios_base::internal & std::ios_base::adjustfield));
    boost::io::ios_precision_saver iprs(output, 9);
    boost::io::ios_fill_saver ifis(output, '@');
    output << "Hello world";
    boost::io::ios_width_saver iws(output, 12);
    output << -16 + 34.5678901234;
    std::locale loc(std::locale::classic(), new backward_bool_names);
    boost::io::ios_locale_saver ils(output, loc);
    output << true;
    BOOST_TEST(&err == output.pword(index));
    BOOST_TEST(69L == output.iword(index));
    try {
        boost::io::ios_exception_saver ies(output, std::ios_base::eofbit);
        boost::io::ios_iostate_saver ias(output,
            output.rdstate() | std::ios_base::eofbit );
        BOOST_ERROR("previous line should have thrown");
#if defined(BOOST_GCC) || (defined(BOOST_CLANG) && defined(BOOST_GNU_STDLIB))
    } catch (std::exception&) {
#else
    } catch (std::ios_base::failure&) {
#endif
        BOOST_TEST(output.exceptions() == std::ios_base::goodbit);
    }
}

int main()
{
    int index = std::ios_base::xalloc();
    std::ostringstream out;
    std::ostringstream err;
    std::istringstream in;
    std::ios_base::fmtflags out_flags = out.flags();
    std::streamsize         out_precision = out.precision();
    std::streamsize         out_width = out.width();
    std::ios_base::iostate  out_iostate = out.rdstate();
    std::ios_base::iostate  out_exceptions = out.exceptions();
    std::ostream*           in_tie = in.tie();
    std::streambuf*         out_sb = out.rdbuf();
    char                    out_fill = out.fill();
    std::locale             out_locale = out.getloc();
    out.iword(index) = 42L;
    out.pword(index) = &in;
    saver_tests_1(index, in, out, err);
    BOOST_TEST(&in == out.pword(index));
    BOOST_TEST(42L == out.iword(index));
    BOOST_TEST(out_locale == out.getloc());
    BOOST_TEST(out_fill == out.fill());
    BOOST_TEST(out_sb == out.rdbuf());
    BOOST_TEST(in_tie == in.tie());
    BOOST_TEST(out_exceptions == out.exceptions());
    BOOST_TEST(out_iostate == out.rdstate());
    BOOST_TEST(out_width == out.width());
    BOOST_TEST(out_precision == out.precision());
    BOOST_TEST(out_flags == out.flags());
    saver_tests_2(index, in, out, err);
    BOOST_TEST(&in == out.pword(index));
    BOOST_TEST(42L == out.iword(index));
    BOOST_TEST(out_locale == out.getloc());
    BOOST_TEST(out_fill == out.fill());
    BOOST_TEST(out_sb == out.rdbuf());
    BOOST_TEST(in_tie == in.tie());
    BOOST_TEST(out_exceptions == out.exceptions());
    BOOST_TEST(out_iostate == out.rdstate());
    BOOST_TEST(out_width == out.width());
    BOOST_TEST(out_precision == out.precision());
    BOOST_TEST(out_flags == out.flags());
    return boost::report_errors();
}
