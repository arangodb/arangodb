/*
Copyright 2003 Daryle Walker

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
#include <iostream>
#include <istream>
#include <locale>
#if defined(BOOST_GCC) || (defined(BOOST_CLANG) && defined(BOOST_GNU_STDLIB))
#include <stdexcept>
#endif
#include <sstream>
#include <cstddef>

class backward_bool_names
    : public std::numpunct<char> {
    typedef std::numpunct<char> base_type;

public:
    explicit backward_bool_names(std::size_t refs = 0)
        : base_type(refs) { }

protected:
    ~backward_bool_names() { }

    base_type::string_type do_truename() const {
        return "eurt";
    }

    base_type::string_type do_falsename() const {
        return "eslaf";
    }
};

void
ios_flags_saver_unit_test()
{
    std::stringstream ss;
    BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
    {
        boost::io::ios_flags_saver ifs(ss);
        BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
        ss << std::noskipws << std::fixed << std::boolalpha;
        BOOST_TEST_EQ(std::ios_base::boolalpha |
            std::ios_base::dec |
            std::ios_base::fixed, ss.flags());
    }
    BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
    {
        boost::io::ios_flags_saver ifs(ss,
            std::ios_base::showbase | std::ios_base::internal);
        BOOST_TEST_EQ(std::ios_base::showbase |
            std::ios_base::internal, ss.flags());
        ss << std::setiosflags(std::ios_base::unitbuf);
        BOOST_TEST_EQ(std::ios_base::showbase |
            std::ios_base::internal |
            std::ios_base::unitbuf, ss.flags());
    }
    BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
}

void
ios_precision_saver_unit_test()
{
    std::stringstream ss;
    BOOST_TEST_EQ(6, ss.precision());
    {
        boost::io::ios_precision_saver ips(ss);
        BOOST_TEST_EQ(6, ss.precision());
        ss << std::setprecision(4);
        BOOST_TEST_EQ(4, ss.precision());
    }
    BOOST_TEST_EQ(6, ss.precision());
    {
        boost::io::ios_precision_saver ips(ss, 8);
        BOOST_TEST_EQ(8, ss.precision());
        ss << std::setprecision(10);
        BOOST_TEST_EQ(10, ss.precision());
    }
    BOOST_TEST_EQ(6, ss.precision());
}

void
ios_width_saver_unit_test()
{
    std::stringstream ss;
    BOOST_TEST_EQ(0, ss.width());
    {
        boost::io::ios_width_saver iws(ss);
        BOOST_TEST_EQ(0, ss.width());
        ss << std::setw(4);
        BOOST_TEST_EQ(4, ss.width());
    }
    BOOST_TEST_EQ(0, ss.width());
    {
        boost::io::ios_width_saver iws(ss, 8);
        BOOST_TEST_EQ(8, ss.width());
        ss << std::setw(10);
        BOOST_TEST_EQ(10, ss.width());
    }
    BOOST_TEST_EQ(0, ss.width());
}

void
ios_iostate_saver_unit_test()
{
    std::stringstream ss;
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.rdstate());
    BOOST_TEST(ss.good());
    {
        boost::io::ios_iostate_saver iis(ss);
        char c;
        BOOST_TEST_EQ(std::ios_base::goodbit, ss.rdstate());
        BOOST_TEST(ss.good());
        ss >> c;
        BOOST_TEST_EQ(std::ios_base::eofbit | std::ios_base::failbit,
            ss.rdstate());
        BOOST_TEST(ss.eof());
        BOOST_TEST(ss.fail());
        BOOST_TEST(!ss.bad());
    }
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.rdstate());
    BOOST_TEST(ss.good());
    {
        boost::io::ios_iostate_saver iis(ss, std::ios_base::eofbit);
        BOOST_TEST_EQ(std::ios_base::eofbit, ss.rdstate());
        BOOST_TEST(ss.eof());
        ss.setstate(std::ios_base::badbit);
        BOOST_TEST_EQ(std::ios_base::eofbit | std::ios_base::badbit,
            ss.rdstate());
        BOOST_TEST(ss.eof());
        BOOST_TEST(ss.fail());
        BOOST_TEST(ss.bad());
    }
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.rdstate());
    BOOST_TEST(ss.good());
}

void
ios_exception_saver_unit_test()
{
    std::stringstream ss;
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.exceptions());
    {
        boost::io::ios_exception_saver ies(ss);
        BOOST_TEST_EQ(std::ios_base::goodbit, ss.exceptions());
        ss.exceptions(std::ios_base::failbit);
        BOOST_TEST_EQ(std::ios_base::failbit, ss.exceptions());
        {
            boost::io::ios_iostate_saver iis(ss);
            ss.exceptions(std::ios_base::failbit | std::ios_base::badbit);
            char c;
#if defined(BOOST_GCC) || (defined(BOOST_CLANG) && defined(BOOST_GNU_STDLIB))
            BOOST_TEST_THROWS(ss >> c, std::exception);
#else
            BOOST_TEST_THROWS(ss >> c, std::ios_base::failure);
#endif
        }
    }
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.exceptions());
    {
        boost::io::ios_exception_saver ies(ss, std::ios_base::eofbit);
        BOOST_TEST_EQ(std::ios_base::eofbit, ss.exceptions());
        ss.exceptions(std::ios_base::badbit);
        BOOST_TEST_EQ(std::ios_base::badbit, ss.exceptions());
        {
            boost::io::ios_iostate_saver iis(ss);
            char c;
            BOOST_TEST_NOT(ss >> c);
        }
    }
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.exceptions());
}

void
ios_tie_saver_unit_test()
{
    BOOST_TEST(NULL == std::cout.tie());
    {
        boost::io::ios_tie_saver its(std::cout);
        BOOST_TEST(NULL == std::cout.tie());
        std::cout.tie(&std::clog);
        BOOST_TEST_EQ(&std::clog, std::cout.tie());
    }
    BOOST_TEST(NULL == std::cout.tie());
    {
        boost::io::ios_tie_saver its(std::cout, &std::clog);
        BOOST_TEST_EQ(&std::clog, std::cout.tie());
        std::cout.tie(&std::cerr);
        BOOST_TEST_EQ(&std::cerr, std::cout.tie());
    }
    BOOST_TEST(NULL == std::cout.tie());
}

void
ios_rdbuf_saver_unit_test()
{
    std::iostream s(NULL);
    BOOST_TEST(NULL == s.rdbuf());
    {
        std::stringbuf sb;
        boost::io::ios_rdbuf_saver irs(s);
        BOOST_TEST(NULL == s.rdbuf());
        s.rdbuf(&sb);
        BOOST_TEST_EQ(&sb, s.rdbuf());
    }
    BOOST_TEST(NULL == s.rdbuf());
    {
        std::stringbuf sb1, sb2("Hi there");
        boost::io::ios_rdbuf_saver irs(s, &sb1);
        BOOST_TEST_EQ(&sb1, s.rdbuf());
        s.rdbuf(&sb2);
        BOOST_TEST_EQ(&sb2, s.rdbuf());
    }
    BOOST_TEST(NULL == s.rdbuf());
}

void
ios_fill_saver_unit_test()
{
    std::stringstream ss;
    BOOST_TEST_EQ(' ',  ss.fill());
    {
        boost::io::ios_fill_saver ifs(ss);
        BOOST_TEST_EQ(' ', ss.fill());
        ss.fill('x');
        BOOST_TEST_EQ('x', ss.fill());
    }
    BOOST_TEST_EQ(' ', ss.fill());
    {
        boost::io::ios_fill_saver ifs(ss, '3');
        BOOST_TEST_EQ('3', ss.fill());
        ss.fill('+');
        BOOST_TEST_EQ('+', ss.fill());
    }
    BOOST_TEST_EQ(' ', ss.fill());
}

void
ios_locale_saver_unit_test()
{
    typedef std::numpunct<char> npc_type;
    std::stringstream ss;
    BOOST_TEST(std::locale() == ss.getloc());
    BOOST_TEST(!std::has_facet<npc_type>(ss.getloc()) ||
        !dynamic_cast<const backward_bool_names*>(&
        std::use_facet<npc_type>(ss.getloc())));
    {
        boost::io::ios_locale_saver ils(ss);
        BOOST_TEST(std::locale() == ss.getloc());
        ss.imbue(std::locale::classic());
        BOOST_TEST(std::locale::classic() == ss.getloc());
    }
    BOOST_TEST(std::locale() == ss.getloc());
    BOOST_TEST(!std::has_facet<npc_type>(ss.getloc()) ||
        !dynamic_cast<const backward_bool_names*>(&
        std::use_facet<npc_type>(ss.getloc())));
    {
        boost::io::ios_locale_saver ils(ss, std::locale::classic());
        BOOST_TEST(std::locale::classic() == ss.getloc());
        BOOST_TEST(!std::has_facet<npc_type>(ss.getloc()) ||
            !dynamic_cast<const backward_bool_names*>(&
            std::use_facet<npc_type>(ss.getloc())));
        ss.imbue(std::locale(std::locale::classic(), new backward_bool_names));
        BOOST_TEST(std::locale::classic() != ss.getloc());
        BOOST_TEST(std::has_facet<npc_type>(ss.getloc()) &&
            dynamic_cast<const backward_bool_names*>(&
            std::use_facet<npc_type>(ss.getloc())));
    }
    BOOST_TEST(std::locale() == ss.getloc());
    BOOST_TEST(!std::has_facet<npc_type>(ss.getloc()) ||
        !dynamic_cast<const backward_bool_names*>(&
        std::use_facet<npc_type>(ss.getloc())));
}

void
ios_iword_saver_unit_test(int index)
{
    std::stringstream ss;
    BOOST_TEST_EQ(0, ss.iword(index));
    {
        boost::io::ios_iword_saver iis(ss, index);
        BOOST_TEST_EQ(0, ss.iword(index));
        ss.iword(index) = 6;
        BOOST_TEST_EQ(6, ss.iword(index));
    }
    BOOST_TEST_EQ(0, ss.iword(index));
    {
        boost::io::ios_iword_saver iis(ss, index, 100);
        BOOST_TEST_EQ(100, ss.iword(index));
        ss.iword(index) = -2000;
        BOOST_TEST_EQ(-2000, ss.iword(index));
    }
    BOOST_TEST_EQ(0, ss.iword(index));
}

void
ios_pword_saver_unit_test(int index)
{
    std::stringstream ss;
    BOOST_TEST(NULL == ss.pword(index));
    {
        boost::io::ios_pword_saver ips(ss, index);
        BOOST_TEST(NULL == ss.pword(index));
        ss.pword(index) = &ss;
        BOOST_TEST_EQ(&ss, ss.pword(index));
    }
    BOOST_TEST(NULL == ss.pword(index));
    {
        boost::io::ios_pword_saver ips(ss, index, ss.rdbuf());
        BOOST_TEST_EQ(ss.rdbuf(), ss.pword(index));
        ss.pword(index) = &ss;
        BOOST_TEST_EQ(&ss, ss.pword(index));
    }
    BOOST_TEST(NULL == ss.pword(index));
}

void
ios_base_all_saver_unit_test()
{
    std::stringstream ss;
    BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
    BOOST_TEST_EQ(6, ss.precision());
    BOOST_TEST_EQ(0, ss.width());
    {
        boost::io::ios_base_all_saver ibas(ss);
        BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
        BOOST_TEST_EQ(6, ss.precision());
        BOOST_TEST_EQ(0, ss.width());
        ss << std::hex << std::unitbuf << std::setprecision(5) << std::setw(7);
        BOOST_TEST_EQ(std::ios_base::unitbuf |
            std::ios_base::hex |
            std::ios_base::skipws, ss.flags());
        BOOST_TEST_EQ(5, ss.precision());
        BOOST_TEST_EQ(7, ss.width());
    }
    BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
    BOOST_TEST_EQ(6, ss.precision());
    BOOST_TEST_EQ(0, ss.width());
}

void
ios_all_saver_unit_test()
{
    typedef std::numpunct<char> npc_type;
    std::stringbuf sb;
    std::iostream ss(&sb);
    BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
    BOOST_TEST_EQ(6, ss.precision());
    BOOST_TEST_EQ(0, ss.width());
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.rdstate());
    BOOST_TEST(ss.good());
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.exceptions());
    BOOST_TEST(NULL == ss.tie());
    BOOST_TEST(&sb == ss.rdbuf());
    BOOST_TEST_EQ(' ',  ss.fill());
    BOOST_TEST(std::locale() == ss.getloc());
    {
        boost::io::ios_all_saver ias(ss);
        BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
        BOOST_TEST_EQ(6, ss.precision());
        BOOST_TEST_EQ(0, ss.width());
        BOOST_TEST_EQ(std::ios_base::goodbit, ss.rdstate());
        BOOST_TEST(ss.good());
        BOOST_TEST_EQ(std::ios_base::goodbit, ss.exceptions());
        BOOST_TEST(NULL == ss.tie());
        BOOST_TEST(&sb == ss.rdbuf());
        BOOST_TEST_EQ(' ', ss.fill());
        BOOST_TEST(std::locale() == ss.getloc());
        ss << std::oct << std::showpos << std::noskipws;
        BOOST_TEST_EQ(std::ios_base::showpos | std::ios_base::oct, ss.flags());
        ss << std::setprecision(3);
        BOOST_TEST_EQ(3, ss.precision());
        ss << std::setw(9);
        BOOST_TEST_EQ(9, ss.width());
        ss.setstate(std::ios_base::eofbit);
        BOOST_TEST_EQ(std::ios_base::eofbit, ss.rdstate());
        BOOST_TEST(ss.eof());
        ss.exceptions(std::ios_base::failbit);
        BOOST_TEST_EQ(std::ios_base::failbit, ss.exceptions());
        {
            boost::io::ios_iostate_saver iis(ss);
            ss.exceptions(std::ios_base::failbit | std::ios_base::badbit);
            char c;
#if defined(BOOST_GCC) || (defined(BOOST_CLANG) && defined(BOOST_GNU_STDLIB))
            BOOST_TEST_THROWS(ss >> c, std::exception);
#else
            BOOST_TEST_THROWS(ss >> c, std::ios_base::failure);
#endif
        }
        ss.tie(&std::clog);
        BOOST_TEST_EQ(&std::clog, ss.tie());
        ss.rdbuf(std::cerr.rdbuf());
        BOOST_TEST_EQ(std::cerr.rdbuf(), ss.rdbuf());
        ss << std::setfill('x');
        BOOST_TEST_EQ('x', ss.fill());
        ss.imbue(std::locale(std::locale::classic(), new backward_bool_names));
        BOOST_TEST(std::locale() != ss.getloc());
        BOOST_TEST(std::has_facet<npc_type>(ss.getloc()) &&
            dynamic_cast<const backward_bool_names*>(&
            std::use_facet<npc_type>(ss.getloc())));
    }
    BOOST_TEST_EQ(std::ios_base::skipws | std::ios_base::dec, ss.flags());
    BOOST_TEST_EQ(6, ss.precision());
    BOOST_TEST_EQ(0, ss.width());
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.rdstate());
    BOOST_TEST(ss.good());
    BOOST_TEST_EQ(std::ios_base::goodbit, ss.exceptions());
    BOOST_TEST(NULL == ss.tie());
    BOOST_TEST(&sb == ss.rdbuf());
    BOOST_TEST_EQ(' ', ss.fill());
    BOOST_TEST(std::locale() == ss.getloc());
}

void
ios_word_saver_unit_test(int index)
{
    std::stringstream ss;
    BOOST_TEST_EQ(0, ss.iword(index));
    BOOST_TEST(NULL == ss.pword(index));
    {
        boost::io::ios_all_word_saver iaws(ss, index);
        BOOST_TEST_EQ(0, ss.iword(index));
        BOOST_TEST(NULL == ss.pword(index));
        ss.iword(index) = -11;
        ss.pword(index) = ss.rdbuf();
        BOOST_TEST_EQ(-11, ss.iword(index));
        BOOST_TEST_EQ(ss.rdbuf(), ss.pword(index));
    }
    BOOST_TEST_EQ(0, ss.iword(index));
    BOOST_TEST(NULL == ss.pword(index));
}

int main()
{
    int index = std::ios_base::xalloc();
    ios_flags_saver_unit_test();
    ios_precision_saver_unit_test();
    ios_width_saver_unit_test();
    ios_iostate_saver_unit_test();
    ios_exception_saver_unit_test();
    ios_tie_saver_unit_test();
    ios_rdbuf_saver_unit_test();
    ios_fill_saver_unit_test();
    ios_locale_saver_unit_test();
    ios_iword_saver_unit_test(index);
    ios_pword_saver_unit_test(index);
    ios_base_all_saver_unit_test();
    ios_all_saver_unit_test();
    ios_word_saver_unit_test(index);
    return boost::report_errors();
}
