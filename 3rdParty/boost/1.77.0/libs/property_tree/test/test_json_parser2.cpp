#include <boost/property_tree/json_parser/detail/parser.hpp>
#include <boost/property_tree/json_parser/detail/narrow_encoding.hpp>
#include <boost/property_tree/json_parser/detail/wide_encoding.hpp>
#include <boost/property_tree/json_parser/detail/standard_callbacks.hpp>
#include "prefixing_callbacks.hpp"

#include <boost/core/lightweight_test.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/range/iterator_range.hpp>

#include <cassert>
#include <sstream>
#include <vector>
#include <algorithm>

using namespace boost::property_tree;

template <typename Ch> struct encoding;
template <> struct encoding<char>
    : json_parser::detail::utf8_utf8_encoding
{};
template <> struct encoding<wchar_t>
    : json_parser::detail::wide_wide_encoding
{};

template <typename Callbacks, typename Ch>
struct test_parser
{
    Callbacks callbacks;
    ::encoding<Ch> encoding;
    typedef std::basic_string<Ch> string;
    typedef basic_ptree<string, string> tree;
    json_parser::detail::parser<Callbacks, ::encoding<Ch>,
                                typename string::const_iterator,
                                typename string::const_iterator>
        parser;

    test_parser() : parser(callbacks, encoding) {}

    bool parse_null(const string& input, string& output) {
        parser.set_input("", input);
        bool result = parser.parse_null();
        if (result) {
            parser.finish();
            output = callbacks.output().data();
        }
        return result;
    }

    bool parse_boolean(const string& input, string& output) {
        parser.set_input("", input);
        bool result = parser.parse_boolean();
        if (result) {
            parser.finish();
            output = callbacks.output().data();
        }
        return result;
    }

    bool parse_number(const string& input, string& output) {
        parser.set_input("", input);
        bool result = parser.parse_number();
        if (result) {
            parser.finish();
            output = callbacks.output().data();
        }
        return result;
    }

    bool parse_string(const string& input, string& output) {
        parser.set_input("", input);
        bool result = parser.parse_string();
        if (result) {
            parser.finish();
            output = callbacks.output().data();
        }
        return result;
    }

    bool parse_array(const string& input, tree& output) {
        parser.set_input("", input);
        bool result = parser.parse_array();
        if (result) {
            parser.finish();
            output = callbacks.output();
        }
        return result;
    }

    bool parse_object(const string& input, tree& output) {
        parser.set_input("", input);
        bool result = parser.parse_object();
        if (result) {
            parser.finish();
            output = callbacks.output();
        }
        return result;
    }

    void parse_value(const string& input, tree& output) {
        parser.set_input("", input);
        parser.parse_value();
        parser.finish();
        output = callbacks.output();
    }
};

template <typename Ch>
struct standard_parser
    : test_parser<
        json_parser::detail::standard_callbacks<
            basic_ptree<std::basic_string<Ch>, std::basic_string<Ch> > >,
        Ch>
{};

template <typename Ch>
struct prefixing_parser
    : test_parser<
        prefixing_callbacks<
            basic_ptree<std::basic_string<Ch>, std::basic_string<Ch> > >,
        Ch>
{};

#define BOM_N "\xef\xbb\xbf"
#define BOM_W L"\xfeff"

static void
test_null_parse_result_is_input()
{
    std::string parsed;
    standard_parser<char> p;
    BOOST_TEST(p.parse_null("null", parsed));
    BOOST_TEST_EQ("null", parsed);
}

static void
test_uses_traits_from_null()
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_TEST(p.parse_null("null", parsed));
    BOOST_TEST_EQ("_:null", parsed);
}

static void
test_null_parse_skips_bom()
{
    std::string parsed;
    standard_parser<char> p;
    BOOST_TEST(p.parse_null(BOM_N "null", parsed));
    BOOST_TEST_EQ("null", parsed);
}

static void
test_null_parse_result_is_input_w()
{
    std::wstring parsed;
    standard_parser<wchar_t> p;
    BOOST_TEST(p.parse_null(L"null", parsed));
    BOOST_TEST(parsed == L"null");
}

static void
test_uses_traits_from_null_w()
{
    std::wstring parsed;
    prefixing_parser<wchar_t> p;
    BOOST_TEST(p.parse_null(L"null", parsed));
    BOOST_TEST(parsed == L"_:null");
}

static void
test_null_parse_skips_bom_w()
{
    std::wstring parsed;
    standard_parser<wchar_t> p;
    BOOST_TEST(p.parse_null(BOM_W L"null", parsed));
    BOOST_TEST(parsed == L"null");
}

template<std::size_t N>
static void
test_boolean_parse_result_is_input_n(const std::string (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        std::string parsed;
        standard_parser<char> p;
        BOOST_TEST(p.parse_boolean(param[i], parsed));
        BOOST_TEST_EQ(param[i], parsed);
    }
}

const std::string
booleans_n[] = { "true", "false" };

static void
test_uses_traits_from_boolean_n()
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_TEST(p.parse_boolean("true", parsed));
    BOOST_TEST_EQ("b:true", parsed);
}

template<std::size_t N>
static void
test_boolean_parse_result_is_input_w(const std::wstring (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        std::wstring parsed;
        standard_parser<wchar_t> p;
        BOOST_TEST(p.parse_boolean(param[i], parsed));
        BOOST_TEST(param[i] == parsed);
    }
}

const std::wstring
booleans_w[] = { L"true", L"false" };

static void
test_uses_traits_from_boolean_w()
{
    std::wstring parsed;
    prefixing_parser<wchar_t> p;
    BOOST_TEST(p.parse_boolean(L"true", parsed));
    BOOST_TEST(parsed == L"b:true");
}

template<std::size_t N>
static void
test_number_parse_result_is_input_n(std::string const (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        std::string parsed;
        standard_parser<char> p;
        BOOST_TEST(p.parse_number(param[i], parsed));
        BOOST_TEST_EQ(param[i], parsed);
    }
}

std::string const
numbers_n[] = {
    "0",
    "-0",
    "1824",
    "-0.1",
    "123.142",
    "1e+0",
    "1E-0",
    "1.1e134"
};

static void
test_uses_traits_from_number_n()
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_TEST(p.parse_number("12345", parsed));
    BOOST_TEST_EQ("n:12345", parsed);
}

template<std::size_t N>
static void
test_number_parse_result_is_input_w(const std::wstring (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        std::wstring parsed;
        standard_parser<wchar_t> p;
        BOOST_TEST(p.parse_number(param[i], parsed));
        BOOST_TEST(parsed == param[i]);
    }
}

std::wstring const numbers_w[] = {
    L"0",
    L"-0",
    L"1824",
    L"-0.1",
    L"123.142",
    L"1e+0",
    L"1E-0",
    L"1.1e134"
};

static void
test_uses_traits_from_number_w()
{
    std::wstring parsed;
    prefixing_parser<wchar_t> p;
    BOOST_TEST(p.parse_number(L"12345", parsed));
    BOOST_TEST(parsed == L"n:12345");
}

struct string_input_n {
    const char* encoded;
    const char* expected;
};

template<std::size_t N>
void test_string_parsed_correctly_n(string_input_n const (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        std::string parsed;
        standard_parser<char> p;
        BOOST_TEST(p.parse_string(param[i].encoded, parsed));
        BOOST_TEST_EQ(param[i].expected, parsed);
    }
}

const string_input_n strings_n[] = {
    {"\"\"", ""},
    {"\"abc\"", "abc"},
    {"\"a\\nb\"", "a\nb"},
    {"\"\\\"\"", "\""},
    {"\"\\\\\"", "\\"},
    {"\"\\/\"", "/"},
    {"\"\\b\"", "\b"},
    {"\"\\f\"", "\f"},
    {"\"\\r\"", "\r"},
    {"\"\\t\"", "\t"},
    {"\"\\u0001\\u00f2\\u28Ec\"", "\x01" "\xC3\xB2" "\xE2\xA3\xAC"},
    {"\"\\ud801\\udc37\"", "\xf0\x90\x90\xb7"}, // U+10437
    {"\xef\xbb\xbf\"\"", ""} // BOM
};

static void
test_uses_string_callbacks()
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_TEST(p.parse_string("\"a\"", parsed));
    BOOST_TEST_EQ("s:a", parsed);
}

struct string_input_w {
    const wchar_t* encoded;
    const wchar_t* expected;
};

template<std::size_t N>
void
test_string_parsed_correctly_w(string_input_w const (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        std::wstring parsed;
        standard_parser<wchar_t> p;
        if(BOOST_TEST(p.parse_string(param[i].encoded, parsed)))
            BOOST_TEST(param[i].expected == parsed);
    }
}

const string_input_w strings_w[] = {
    {L"\"\"", L""},
    {L"\"abc\"", L"abc"},
    {L"\"a\\nb\"", L"a\nb"},
    {L"\"\\\"\"", L"\""},
    {L"\"\\\\\"", L"\\"},
    {L"\"\\/\"", L"/"},
    {L"\"\\b\"", L"\b"},
    {L"\"\\f\"", L"\f"},
    {L"\"\\r\"", L"\r"},
    {L"\"\\t\"", L"\t"},
    {L"\"\\u0001\\u00f2\\u28Ec\"", L"\x0001" L"\x00F2" L"\x28EC"},
    {L"\xfeff\"\"", L""} // BOM
};

static void 
test_empty_array()
{
    ptree tree;
    standard_parser<char> p;
    const char* input = " [ ]";
    BOOST_TEST(p.parse_array(input, tree));
    BOOST_TEST_EQ("", tree.data());
    BOOST_TEST_EQ(0u, tree.size());
}

static void
test_array_gets_tagged()
{
    wptree tree;
    prefixing_parser<wchar_t> p;
    const wchar_t* input = L" [ ]";
    BOOST_TEST(p.parse_array(input, tree));
    BOOST_TEST(tree.data() == L"a:");
    BOOST_TEST_EQ(0u, tree.size());
}

static void
test_array_with_values()
{
    wptree tree;
    standard_parser<wchar_t> p;
    const wchar_t* input = L"[\n"
L"      123, \"abc\" ,true ,\n"
L"      null\n"
L"  ]";
    if(!BOOST_TEST(p.parse_array(input, tree))) 
        return;
    if(!BOOST_TEST_EQ(4u, tree.size()))
        return;
    wptree::iterator it = tree.begin();
    BOOST_TEST(it->first == L"");
    BOOST_TEST(it->second.data() == L"123");
    ++it;
    BOOST_TEST(it->first == L"");
    BOOST_TEST(it->second.data() == L"abc");
    ++it;
    BOOST_TEST(it->first == L"");
    BOOST_TEST(it->second.data() == L"true");
    ++it;
    BOOST_TEST(it->first == L"");
    BOOST_TEST(it->second.data() == L"null");
    ++it;
    BOOST_TEST(tree.end() == it);
}

static void
test_array_values_get_tagged()
{
    ptree tree;
    prefixing_parser<char> p;
    const char* input = "[\n"
"       123, \"abc\" ,true ,\n"
"       null\n"
"   ]";
    if(BOOST_TEST(p.parse_array(input, tree)))
        if(BOOST_TEST_EQ(4u, tree.size()))
        {
            BOOST_TEST_EQ("a:", tree.data());
            ptree::iterator it = tree.begin();
            BOOST_TEST_EQ("", it->first);
            BOOST_TEST_EQ("n:123", it->second.data());
            ++it;
            BOOST_TEST_EQ("", it->first);
            BOOST_TEST_EQ("s:abc", it->second.data());
            ++it;
            BOOST_TEST_EQ("", it->first);
            BOOST_TEST_EQ("b:true", it->second.data());
            ++it;
            BOOST_TEST_EQ("", it->first);
            BOOST_TEST_EQ("_:null", it->second.data());
            ++it;
            BOOST_TEST(tree.end() == it);
        }
}

static void
test_nested_array()
{
    ptree tree;
    standard_parser<char> p;
    const char* input = "[[1,2],3,[4,5]]";
    if(!BOOST_TEST(p.parse_array(input, tree)))
        return;
    if(!BOOST_TEST_EQ(3u, tree.size()))
        return;
    ptree::iterator it = tree.begin();
    BOOST_TEST_EQ("", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(!BOOST_TEST_EQ(2u, sub.size()))
            return;
        ptree::iterator iit = sub.begin();
        BOOST_TEST_EQ("", iit->first);
        BOOST_TEST_EQ("1", iit->second.data());
        ++iit;
        BOOST_TEST_EQ("", iit->first);
        BOOST_TEST_EQ("2", iit->second.data());
        ++iit;
        BOOST_TEST(sub.end() == iit);
    }
    ++it;
    BOOST_TEST_EQ("", it->first);
    BOOST_TEST_EQ("3", it->second.data());
    ++it;
    BOOST_TEST_EQ("", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(!BOOST_TEST_EQ(2u, sub.size()))
            return;
        ptree::iterator iit = sub.begin();
        BOOST_TEST_EQ("", iit->first);
        BOOST_TEST_EQ("4", iit->second.data());
        ++iit;
        BOOST_TEST_EQ("", iit->first);
        BOOST_TEST_EQ("5", iit->second.data());
        ++iit;
        BOOST_TEST(sub.end() == iit);
    }
    ++it;
    BOOST_TEST(tree.end() == it);
}

static void
test_empty_object()
{
    ptree tree;
    standard_parser<char> p;
    const char* input = " { }";
    if(BOOST_TEST(p.parse_object(input, tree)))
    {
        BOOST_TEST_EQ("", tree.data());
        BOOST_TEST_EQ(0u, tree.size());
    }
}

static void
test_object_gets_tagged()
{
    wptree tree;
    prefixing_parser<wchar_t> p;
    const wchar_t* input = L" { }";
    if(BOOST_TEST(p.parse_object(input, tree)))
    {
        BOOST_TEST(tree.data() == L"o:");
        BOOST_TEST_EQ(0u, tree.size());
    }
}

static void
test_object_with_values()
{
    wptree tree;
    standard_parser<wchar_t> p;
    const wchar_t* input = L"{\n"
L"      \"1\":123, \"2\"\n"
L"            :\"abc\" ,\"3\": true ,\n"
L"      \"4\"   : null\n"
L"  }";
    if(BOOST_TEST(p.parse_object(input, tree)))
        if(BOOST_TEST_EQ(4u, tree.size()))
        {
            wptree::iterator it = tree.begin();
            BOOST_TEST(it->first == L"1");
            BOOST_TEST(it->second.data() == L"123");
            ++it;
            BOOST_TEST(it->first == L"2");
            BOOST_TEST(it->second.data() == L"abc");
            ++it;
            BOOST_TEST(it->first == L"3");
            BOOST_TEST(it->second.data() == L"true");
            ++it;
            BOOST_TEST(it->first == L"4");
            BOOST_TEST(it->second.data() == L"null");
            ++it;
            BOOST_TEST(tree.end() == it);
        }
}

static void
test_object_values_get_tagged()
{
    ptree tree;
    prefixing_parser<char> p;
    const char* input = "{\n"
        "\"1\": 123, \"2\": \"abc\" ,\"3\": true ,\n"
        "\"4\": null\n"
    "}";
    if(BOOST_TEST(p.parse_object(input, tree)))
        if(BOOST_TEST_EQ(4u, tree.size()))
        {
            BOOST_TEST_EQ("o:", tree.data());
            ptree::iterator it = tree.begin();
            BOOST_TEST_EQ("1", it->first);
            BOOST_TEST_EQ("n:123", it->second.data());
            ++it;
            BOOST_TEST_EQ("2", it->first);
            BOOST_TEST_EQ("s:abc", it->second.data());
            ++it;
            BOOST_TEST_EQ("3", it->first);
            BOOST_TEST_EQ("b:true", it->second.data());
            ++it;
            BOOST_TEST_EQ("4", it->first);
            BOOST_TEST_EQ("_:null", it->second.data());
            ++it;
            BOOST_TEST(tree.end() == it);
        }
}

static void
test_nested_object()
{
    ptree tree;
    standard_parser<char> p;
    const char* input = "{\"a\":{\"b\":1,\"c\":2},\"d\":3,\"e\":{\"f\":4,\"g\":5}}";
    if(!BOOST_TEST(p.parse_object(input, tree)))
        return;
    if(!BOOST_TEST_EQ(3u, tree.size()))
        return;
    ptree::iterator it = tree.begin();
    BOOST_TEST_EQ("a", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(!BOOST_TEST_EQ(2u, sub.size()))
            return;
        ptree::iterator iit = sub.begin();
        BOOST_TEST_EQ("b", iit->first);
        BOOST_TEST_EQ("1", iit->second.data());
        ++iit;
        BOOST_TEST_EQ("c", iit->first);
        BOOST_TEST_EQ("2", iit->second.data());
        ++iit;
        BOOST_TEST(sub.end() == iit);
    }
    ++it;
    BOOST_TEST_EQ("d", it->first);
    BOOST_TEST_EQ("3", it->second.data());
    ++it;
    BOOST_TEST_EQ("e", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(!BOOST_TEST_EQ(2u, sub.size()))
            return;
        ptree::iterator iit = sub.begin();
        BOOST_TEST_EQ("f", iit->first);
        BOOST_TEST_EQ("4", iit->second.data());
        ++iit;
        BOOST_TEST_EQ("g", iit->first);
        BOOST_TEST_EQ("5", iit->second.data());
        ++iit;
        BOOST_TEST(sub.end() == iit);
    }
    ++it;
    BOOST_TEST(tree.end() == it);
}

static void
test_array_in_object()
{
    ptree tree;
    standard_parser<char> p;
    const char* input = "{\"a\":[1,2],\"b\":3,\"c\":[4,5]}";
    if(!BOOST_TEST(p.parse_object(input, tree)))
        return;
    if(!BOOST_TEST_EQ(3u, tree.size()))
        return;
    ptree::iterator it = tree.begin();
    BOOST_TEST_EQ("a", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(BOOST_TEST_EQ(2u, sub.size()))
        {
            ptree::iterator iit = sub.begin();
            BOOST_TEST_EQ("", iit->first);
            BOOST_TEST_EQ("1", iit->second.data());
            ++iit;
            BOOST_TEST_EQ("", iit->first);
            BOOST_TEST_EQ("2", iit->second.data());
            ++iit;
            BOOST_TEST(sub.end() == iit);
        }
    }
    ++it;
    BOOST_TEST_EQ("b", it->first);
    BOOST_TEST_EQ("3", it->second.data());
    ++it;
    BOOST_TEST_EQ("c", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(BOOST_TEST_EQ(2u, sub.size()))
        {
            ptree::iterator iit = sub.begin();
            BOOST_TEST_EQ("", iit->first);
            BOOST_TEST_EQ("4", iit->second.data());
            ++iit;
            BOOST_TEST_EQ("", iit->first);
            BOOST_TEST_EQ("5", iit->second.data());
            ++iit;
            BOOST_TEST(sub.end() == iit);
        }
    }
    ++it;
    BOOST_TEST(tree.end() == it);
}

static void
test_object_in_array()
{
    ptree tree;
    standard_parser<char> p;
    const char* input = "[{\"a\":1,\"b\":2},3,{\"c\":4,\"d\":5}]";
    if(!BOOST_TEST(p.parse_array(input, tree)))
        return;
    if(!BOOST_TEST_EQ(3u, tree.size()))
        return;
    ptree::iterator it = tree.begin();
    BOOST_TEST_EQ("", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(BOOST_TEST_EQ(2u, sub.size()))
        {
            ptree::iterator iit = sub.begin();
            BOOST_TEST_EQ("a", iit->first);
            BOOST_TEST_EQ("1", iit->second.data());
            ++iit;
            BOOST_TEST_EQ("b", iit->first);
            BOOST_TEST_EQ("2", iit->second.data());
            ++iit;
            BOOST_TEST(sub.end() == iit);
        }
    }
    ++it;
    BOOST_TEST_EQ("", it->first);
    BOOST_TEST_EQ("3", it->second.data());
    ++it;
    BOOST_TEST_EQ("", it->first);
    {
        ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(BOOST_TEST_EQ(2u, sub.size()))
        {
            ptree::iterator iit = sub.begin();
            BOOST_TEST_EQ("c", iit->first);
            BOOST_TEST_EQ("4", iit->second.data());
            ++iit;
            BOOST_TEST_EQ("d", iit->first);
            BOOST_TEST_EQ("5", iit->second.data());
            ++iit;
            BOOST_TEST(sub.end() == iit);
        }
    }
    ++it;
    BOOST_TEST(tree.end() == it);
}

static void
test_parser_works_with_input_iterators()
{
    const char* input = " {\n"
"       \"1\":123, \"2\"\n"
"            :\"abc\" ,\"3\": true ,\n"
"       \"4\"   : null, \"5\" : [ 1, 23\n"
"            , 456 ]\n"
"   }";

    std::istringstream is(input);
    typedef std::istreambuf_iterator<char> iterator;
    json_parser::detail::standard_callbacks<ptree> callbacks;
    json_parser::detail::utf8_utf8_encoding encoding;
    json_parser::detail::parser<json_parser::detail::standard_callbacks<ptree>,
                                json_parser::detail::utf8_utf8_encoding,
                                iterator, iterator>
        p(callbacks, encoding);

    p.set_input("", boost::make_iterator_range(iterator(is), iterator()));
    p.parse_value();

    const ptree& tree = callbacks.output();
    if(!BOOST_TEST_EQ(5u, tree.size()))
        return;
    ptree::const_iterator it = tree.begin();
    BOOST_TEST_EQ("1", it->first);
    BOOST_TEST_EQ("123", it->second.data());
    ++it;
    BOOST_TEST_EQ("2", it->first);
    BOOST_TEST_EQ("abc", it->second.data());
    ++it;
    BOOST_TEST_EQ("3", it->first);
    BOOST_TEST_EQ("true", it->second.data());
    ++it;
    BOOST_TEST_EQ("4", it->first);
    BOOST_TEST_EQ("null", it->second.data());
    ++it;
    BOOST_TEST_EQ("5", it->first);
    {
        const ptree& sub = it->second;
        BOOST_TEST_EQ("", sub.data());
        if(!BOOST_TEST_EQ(3u, sub.size()))
            return;
        ptree::const_iterator iit = sub.begin();
        BOOST_TEST_EQ("", iit->first);
        BOOST_TEST_EQ("1", iit->second.data());
        ++iit;
        BOOST_TEST_EQ("", iit->first);
        BOOST_TEST_EQ("23", iit->second.data());
        ++iit;
        BOOST_TEST_EQ("", iit->first);
        BOOST_TEST_EQ("456", iit->second.data());
        ++iit;
        BOOST_TEST(sub.end() == iit);
    }
    ++it;
    BOOST_TEST(tree.end() == it);
}

struct bad_parse_n {
    const char* json;
    const char* message_substring;
};

template<std::size_t N>
void test_parse_error_thrown_with_message_n(bad_parse_n const (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        try {
            standard_parser<char> p;
            ptree dummy;
            p.parse_value(param[i].json, dummy);
            BOOST_ERROR("expected exception");
        } catch (json_parser::json_parser_error& e) {
            std::string message = e.message();
            if(message.find(param[i].message_substring) ==
                                    std::string::npos)
            {
                std::ostringstream ss;
                ss << "bad error message on input '" << param[i].json
                    << "', need: '" << param[i].message_substring
                    << "' but found '" << message << "'";
                BOOST_ERROR(ss.str().c_str());
            }
        }
    }
}

const bad_parse_n errors_n[] = {
    {"", "expected value"},
    {"(", "expected value"},

    {"n", "expected 'null'"},
    {"nu", "expected 'null'"},
    {"nul", "expected 'null'"},
    {"n ", "expected 'null'"},
    {"nu ", "expected 'null'"},
    {"nul ", "expected 'null'"},
    {"nx", "expected 'null'"},
    {"nux", "expected 'null'"},
    {"nulx", "expected 'null'"},

    {"t", "expected 'true'"},
    {"tr", "expected 'true'"},
    {"tu", "expected 'true'"},
    {"t ", "expected 'true'"},
    {"tr ", "expected 'true'"},
    {"tru ", "expected 'true'"},
    {"tx", "expected 'true'"},
    {"trx", "expected 'true'"},
    {"trux", "expected 'true'"},

    {"f", "expected 'false'"},
    {"fa", "expected 'false'"},
    {"fal", "expected 'false'"},
    {"fals", "expected 'false'"},
    {"f ", "expected 'false'"},
    {"fa ", "expected 'false'"},
    {"fal ", "expected 'false'"},
    {"fals ", "expected 'false'"},
    {"fx", "expected 'false'"},
    {"fax", "expected 'false'"},
    {"falx", "expected 'false'"},
    {"falsx", "expected 'false'"},

    {"-", "expected digits"},
    {"01", "garbage after data"},
    {"0.", "need at least one digit after '.'"},
    {"0e", "need at least one digit in exponent"},
    {"0e-", "need at least one digit in exponent"},

    {"\"", "unterminated string"},
    {"\"asd", "unterminated string"},
    {"\"\n\"", "invalid code sequence"}, // control character
    {"\"\xff\"", "invalid code sequence"}, // bad lead byte
    {"\"\x80\"", "invalid code sequence"}, // stray trail byte
    {"\"\xc0", "invalid code sequence"}, // eos after lead byte
    {"\"\xc0\"", "invalid code sequence"}, // bad trail byte
    {"\"\xc0m\"", "invalid code sequence"}, // also bad trail byte
    {"\"\\", "invalid escape sequence"},
    {"\"\\p\"", "invalid escape sequence"},
    {"\"\\u", "invalid escape sequence"},
    {"\"\\u\"", "invalid escape sequence"},
    {"\"\\ug\"", "invalid escape sequence"},
    {"\"\\u1\"", "invalid escape sequence"},
    {"\"\\u1g\"", "invalid escape sequence"},
    {"\"\\u11\"", "invalid escape sequence"},
    {"\"\\u11g\"", "invalid escape sequence"},
    {"\"\\u111\"", "invalid escape sequence"},
    {"\"\\u111g\"", "invalid escape sequence"},
    {"\"\\ude00\"", "stray low surrogate"},
    {"\"\\ud900", "stray high surrogate"},
    {"\"\\ud900foo\"", "stray high surrogate"},
    {"\"\\ud900\\", "expected codepoint reference"},
    {"\"\\ud900\\n\"", "expected codepoint reference"},
    {"\"\\ud900\\u1000\"", "expected low surrogate"},

    {"[", "expected value"},
    {"[1", "expected ']' or ','"},
    {"[1,", "expected value"},
    {"[1,]", "expected value"},
    {"[1}", "expected ']' or ','"},

    {"{", "expected key string"},
    {"{1:2}", "expected key string"},
    {"{\"\"", "expected ':'"},
    {"{\"\"}", "expected ':'"},
    {"{\"\":", "expected value"},
    {"{\"\":}", "expected value"},
    {"{\"\":0", "expected '}' or ','"},
    {"{\"\":0]", "expected '}' or ','"},
    {"{\"\":0,", "expected key string"},
    {"{\"\":0,}", "expected key string"},
};

struct bad_parse_w {
    const wchar_t* json;
    const char* message_substring;
};

struct do_narrow
{
    char operator()(std::wstring::value_type w) const
    {
        unsigned long u = static_cast<unsigned long>(w);
        if (u < 32 || u > 126)
            return '?';
        else
            return static_cast<char>(u);
    }
};
static std::string
make_narrow(std::wstring const& in)
{
    std::string result(in.size(), ' ');
    std::transform(in.begin(), in.end(), result.begin(), do_narrow());
    return result;
}

template<std::size_t N>
void
test_parse_error_thrown_with_message_w(bad_parse_w const (&param)[N])
{
    for(std::size_t i = 0 ; i < N ; ++i)
    {
        try {
            standard_parser<wchar_t> p;
            wptree dummy;
            p.parse_value(param[i].json, dummy);
            BOOST_ERROR("expected exception");
        } catch (json_parser::json_parser_error& e) {
            std::string message = e.message();
            if (message.find(param[i].message_substring) ==
                                    std::string::npos)
            {
                std::ostringstream ss;
                ss << "bad error message on input '" << make_narrow(param[i].json)
                                    << "', need: '" << param[i].message_substring
                                    << "' but found '" << message << "'";
                BOOST_ERROR(ss.str().c_str());
            }
        }
    }
}

const bad_parse_w
errors_w[] = {
    {L"", "expected value"},
    {L"(", "expected value"},

    {L"n", "expected 'null'"},
    {L"nu", "expected 'null'"},
    {L"nul", "expected 'null'"},
    {L"n ", "expected 'null'"},
    {L"nu ", "expected 'null'"},
    {L"nul ", "expected 'null'"},
    {L"nx", "expected 'null'"},
    {L"nux", "expected 'null'"},
    {L"nulx", "expected 'null'"},

    {L"t", "expected 'true'"},
    {L"tr", "expected 'true'"},
    {L"tu", "expected 'true'"},
    {L"t ", "expected 'true'"},
    {L"tr ", "expected 'true'"},
    {L"tru ", "expected 'true'"},
    {L"tx", "expected 'true'"},
    {L"trx", "expected 'true'"},
    {L"trux", "expected 'true'"},

    {L"f", "expected 'false'"},
    {L"fa", "expected 'false'"},
    {L"fal", "expected 'false'"},
    {L"fals", "expected 'false'"},
    {L"f ", "expected 'false'"},
    {L"fa ", "expected 'false'"},
    {L"fal ", "expected 'false'"},
    {L"fals ", "expected 'false'"},
    {L"fx", "expected 'false'"},
    {L"fax", "expected 'false'"},
    {L"falx", "expected 'false'"},
    {L"falsx", "expected 'false'"},

    {L"-", "expected digits"},
    {L"01", "garbage after data"},
    {L"0.", "need at least one digit after '.'"},
    {L"0e", "need at least one digit in exponent"},
    {L"0e-", "need at least one digit in exponent"},

    {L"\"", "unterminated string"},
    {L"\"asd", "unterminated string"},
    {L"\"\n\"", "invalid code sequence"}, // control character
    // Encoding not known, so no UTF-16 encoding error tests.
    {L"\"\\", "invalid escape sequence"},
    {L"\"\\p\"", "invalid escape sequence"},
    {L"\"\\u", "invalid escape sequence"},
    {L"\"\\u\"", "invalid escape sequence"},
    {L"\"\\ug\"", "invalid escape sequence"},
    {L"\"\\u1\"", "invalid escape sequence"},
    {L"\"\\u1g\"", "invalid escape sequence"},
    {L"\"\\u11\"", "invalid escape sequence"},
    {L"\"\\u11g\"", "invalid escape sequence"},
    {L"\"\\u111\"", "invalid escape sequence"},
    {L"\"\\u111g\"", "invalid escape sequence"},
    {L"\"\\ude00\"", "stray low surrogate"},
    {L"\"\\ud900", "stray high surrogate"},
    {L"\"\\ud900foo\"", "stray high surrogate"},
    {L"\"\\ud900\\", "expected codepoint reference"},
    {L"\"\\ud900\\n\"", "expected codepoint reference"},
    {L"\"\\ud900\\u1000\"", "expected low surrogate"},

    {L"[", "expected value"},
    {L"[1", "expected ']' or ','"},
    {L"[1,", "expected value"},
    {L"[1,]", "expected value"},
    {L"[1}", "expected ']' or ','"},

    {L"{", "expected key string"},
    {L"{1:2}", "expected key string"},
    {L"{\"\"", "expected ':'"},
    {L"{\"\"}", "expected ':'"},
    {L"{\"\":", "expected value"},
    {L"{\"\":}", "expected value"},
    {L"{\"\":0", "expected '}' or ','"},
    {L"{\"\":0]", "expected '}' or ','"},
    {L"{\"\":0,", "expected key string"},
    {L"{\"\":0,}", "expected key string"},
};

int main()
{
    test_null_parse_result_is_input();
    test_uses_traits_from_null();
    test_null_parse_skips_bom();
    test_null_parse_result_is_input_w();
    test_null_parse_skips_bom_w();
    test_uses_traits_from_boolean_n();
    test_uses_traits_from_boolean_w();
    test_boolean_parse_result_is_input_n(booleans_n);
    test_boolean_parse_result_is_input_w(booleans_w);
    test_number_parse_result_is_input_n(numbers_n);
    test_number_parse_result_is_input_w(numbers_w);
    test_uses_traits_from_number_n();
    test_string_parsed_correctly_n(strings_n);
    test_string_parsed_correctly_w(strings_w);
    test_parse_error_thrown_with_message_n(errors_n);
    test_parse_error_thrown_with_message_w(errors_w);
    test_uses_string_callbacks();
    test_empty_array();
    test_array_with_values();
    test_array_values_get_tagged();
    test_nested_array();
    test_empty_object();
    test_object_gets_tagged();
    test_object_with_values();
    test_object_values_get_tagged();
    test_nested_object();
    test_array_in_object();
    test_object_in_array();
    test_parser_works_with_input_iterators();
    test_uses_traits_from_null_w();
    test_uses_traits_from_number_w();
    test_array_gets_tagged();
    return boost::report_errors();
}

/*
test_suite* init_unit_test_suite(int, char*[])
{
    master_test_suite_t& ts = boost::unit_test::framework::master_test_suite();
    ts.add(ARRAY_TEST_CASE(boolean_parse_result_is_input_n, booleans_n));
    ts.add(ARRAY_TEST_CASE(boolean_parse_result_is_input_w, booleans_w));
    ts.add(ARRAY_TEST_CASE(number_parse_result_is_input_n, numbers_n));
    ts.add(ARRAY_TEST_CASE(number_parse_result_is_input_w, numbers_w));
    ts.add(ARRAY_TEST_CASE(string_parsed_correctly_n, strings_n));
    ts.add(ARRAY_TEST_CASE(string_parsed_correctly_w, strings_w));
    ts.add(ARRAY_TEST_CASE(parse_error_thrown_with_message_n, errors_n));
    ts.add(ARRAY_TEST_CASE(parse_error_thrown_with_message_w, errors_w));

    return 0;
}
*/