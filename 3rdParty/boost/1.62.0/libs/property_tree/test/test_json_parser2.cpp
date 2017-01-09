#include <boost/property_tree/json_parser/detail/parser.hpp>
#include <boost/property_tree/json_parser/detail/narrow_encoding.hpp>
#include <boost/property_tree/json_parser/detail/wide_encoding.hpp>
#include <boost/property_tree/json_parser/detail/standard_callbacks.hpp>
#include "prefixing_callbacks.hpp"

#define BOOST_TEST_NO_MAIN
#include <boost/test/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>

#include <boost/property_tree/ptree.hpp>
#include <boost/range/iterator_range.hpp>

#include <cassert>
#include <sstream>
#include <vector>

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

namespace boost { namespace test_tools { namespace tt_detail {
    template<>
    struct print_log_value<std::wstring> {
        void operator()(std::ostream& os, const std::wstring& s) {
            print_log_value<const wchar_t*>()(os, s.c_str());
        }
    };
}}}
BOOST_TEST_DONT_PRINT_LOG_VALUE(ptree::iterator)
BOOST_TEST_DONT_PRINT_LOG_VALUE(ptree::const_iterator)
BOOST_TEST_DONT_PRINT_LOG_VALUE(wptree::iterator)
BOOST_TEST_DONT_PRINT_LOG_VALUE(wptree::const_iterator)

BOOST_AUTO_TEST_CASE(null_parse_result_is_input) {
    std::string parsed;
    standard_parser<char> p;
    BOOST_REQUIRE(p.parse_null("null", parsed));
    BOOST_CHECK_EQUAL("null", parsed);
}

BOOST_AUTO_TEST_CASE(uses_traits_from_null)
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_REQUIRE(p.parse_null("null", parsed));
    BOOST_CHECK_EQUAL("_:null", parsed);
}

BOOST_AUTO_TEST_CASE(null_parse_skips_bom) {
    std::string parsed;
    standard_parser<char> p;
    BOOST_REQUIRE(p.parse_null(BOM_N "null", parsed));
    BOOST_CHECK_EQUAL("null", parsed);
}

BOOST_AUTO_TEST_CASE(null_parse_result_is_input_w) {
    std::wstring parsed;
    standard_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_null(L"null", parsed));
    BOOST_CHECK_EQUAL(L"null", parsed);
}

BOOST_AUTO_TEST_CASE(uses_traits_from_null_w)
{
    std::wstring parsed;
    prefixing_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_null(L"null", parsed));
    BOOST_CHECK_EQUAL(L"_:null", parsed);
}

BOOST_AUTO_TEST_CASE(null_parse_skips_bom_w) {
    std::wstring parsed;
    standard_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_null(BOM_W L"null", parsed));
    BOOST_CHECK_EQUAL(L"null", parsed);
}

void boolean_parse_result_is_input_n(const char* param) {
    std::string parsed;
    standard_parser<char> p;
    BOOST_REQUIRE(p.parse_boolean(param, parsed));
    BOOST_CHECK_EQUAL(param, parsed);
}

const char* const booleans_n[] = { "true", "false" };

BOOST_AUTO_TEST_CASE(uses_traits_from_boolean_n)
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_REQUIRE(p.parse_boolean("true", parsed));
    BOOST_CHECK_EQUAL("b:true", parsed);
}

void boolean_parse_result_is_input_w(const wchar_t* param) {
    std::wstring parsed;
    standard_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_boolean(param, parsed));
    BOOST_CHECK_EQUAL(param, parsed);
}

const wchar_t* const booleans_w[] = { L"true", L"false" };

BOOST_AUTO_TEST_CASE(uses_traits_from_boolean_w)
{
    std::wstring parsed;
    prefixing_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_boolean(L"true", parsed));
    BOOST_CHECK_EQUAL(L"b:true", parsed);
}

void number_parse_result_is_input_n(const char* param) {
    std::string parsed;
    standard_parser<char> p;
    BOOST_REQUIRE(p.parse_number(param, parsed));
    BOOST_CHECK_EQUAL(param, parsed);
}

const char* const numbers_n[] = {
    "0",
    "-0",
    "1824",
    "-0.1",
    "123.142",
    "1e+0",
    "1E-0",
    "1.1e134"
};

BOOST_AUTO_TEST_CASE(uses_traits_from_number_n)
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_REQUIRE(p.parse_number("12345", parsed));
    BOOST_CHECK_EQUAL("n:12345", parsed);
}

void number_parse_result_is_input_w(const wchar_t* param) {
    std::wstring parsed;
    standard_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_number(param, parsed));
    BOOST_CHECK_EQUAL(param, parsed);
}

const wchar_t* const numbers_w[] = {
    L"0",
    L"-0",
    L"1824",
    L"-0.1",
    L"123.142",
    L"1e+0",
    L"1E-0",
    L"1.1e134"
};

BOOST_AUTO_TEST_CASE(uses_traits_from_number_w)
{
    std::wstring parsed;
    prefixing_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_number(L"12345", parsed));
    BOOST_CHECK_EQUAL(L"n:12345", parsed);
}

struct string_input_n {
    const char* encoded;
    const char* expected;
};

void string_parsed_correctly_n(string_input_n param) {
    std::string parsed;
    standard_parser<char> p;
    BOOST_REQUIRE(p.parse_string(param.encoded, parsed));
    BOOST_CHECK_EQUAL(param.expected, parsed);
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

BOOST_AUTO_TEST_CASE(uses_string_callbacks)
{
    std::string parsed;
    prefixing_parser<char> p;
    BOOST_REQUIRE(p.parse_string("\"a\"", parsed));
    BOOST_CHECK_EQUAL("s:a", parsed);
}

struct string_input_w {
    const wchar_t* encoded;
    const wchar_t* expected;
};

void string_parsed_correctly_w(string_input_w param) {
    std::wstring parsed;
    standard_parser<wchar_t> p;
    BOOST_REQUIRE(p.parse_string(param.encoded, parsed));
    BOOST_CHECK_EQUAL(param.expected, parsed);
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

BOOST_AUTO_TEST_CASE(empty_array) {
    ptree tree;
    standard_parser<char> p;
    const char* input = " [ ]";
    BOOST_REQUIRE(p.parse_array(input, tree));
    BOOST_CHECK_EQUAL("", tree.data());
    BOOST_CHECK_EQUAL(0u, tree.size());
}

BOOST_AUTO_TEST_CASE(array_gets_tagged) {
    wptree tree;
    prefixing_parser<wchar_t> p;
    const wchar_t* input = L" [ ]";
    BOOST_REQUIRE(p.parse_array(input, tree));
    BOOST_CHECK_EQUAL(L"a:", tree.data());
    BOOST_CHECK_EQUAL(0u, tree.size());
}

BOOST_AUTO_TEST_CASE(array_with_values) {
    wptree tree;
    standard_parser<wchar_t> p;
    const wchar_t* input = L"[\n"
L"      123, \"abc\" ,true ,\n"
L"      null\n"
L"  ]";
    BOOST_REQUIRE(p.parse_array(input, tree));
    BOOST_REQUIRE_EQUAL(4u, tree.size());
    wptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL(L"", it->first);
    BOOST_CHECK_EQUAL(L"123", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(L"", it->first);
    BOOST_CHECK_EQUAL(L"abc", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(L"", it->first);
    BOOST_CHECK_EQUAL(L"true", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(L"", it->first);
    BOOST_CHECK_EQUAL(L"null", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(array_values_get_tagged) {
    ptree tree;
    prefixing_parser<char> p;
    const char* input = "[\n"
"       123, \"abc\" ,true ,\n"
"       null\n"
"   ]";
    BOOST_REQUIRE(p.parse_array(input, tree));
    BOOST_REQUIRE_EQUAL(4u, tree.size());
    BOOST_CHECK_EQUAL("a:", tree.data());
    ptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL("", it->first);
    BOOST_CHECK_EQUAL("n:123", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("", it->first);
    BOOST_CHECK_EQUAL("s:abc", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("", it->first);
    BOOST_CHECK_EQUAL("b:true", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("", it->first);
    BOOST_CHECK_EQUAL("_:null", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(nested_array) {
    ptree tree;
    standard_parser<char> p;
    const char* input = "[[1,2],3,[4,5]]";
    BOOST_REQUIRE(p.parse_array(input, tree));
    BOOST_REQUIRE_EQUAL(3u, tree.size());
    ptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL("", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("1", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("2", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL("", it->first);
    BOOST_CHECK_EQUAL("3", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("4", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("5", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(empty_object) {
    ptree tree;
    standard_parser<char> p;
    const char* input = " { }";
    BOOST_REQUIRE(p.parse_object(input, tree));
    BOOST_CHECK_EQUAL("", tree.data());
    BOOST_CHECK_EQUAL(0u, tree.size());
}

BOOST_AUTO_TEST_CASE(object_gets_tagged) {
    wptree tree;
    prefixing_parser<wchar_t> p;
    const wchar_t* input = L" { }";
    BOOST_REQUIRE(p.parse_object(input, tree));
    BOOST_CHECK_EQUAL(L"o:", tree.data());
    BOOST_CHECK_EQUAL(0u, tree.size());
}

BOOST_AUTO_TEST_CASE(object_with_values) {
    wptree tree;
    standard_parser<wchar_t> p;
    const wchar_t* input = L"{\n"
L"      \"1\":123, \"2\"\n"
L"            :\"abc\" ,\"3\": true ,\n"
L"      \"4\"   : null\n"
L"  }";
    BOOST_REQUIRE(p.parse_object(input, tree));
    BOOST_REQUIRE_EQUAL(4u, tree.size());
    wptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL(L"1", it->first);
    BOOST_CHECK_EQUAL(L"123", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(L"2", it->first);
    BOOST_CHECK_EQUAL(L"abc", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(L"3", it->first);
    BOOST_CHECK_EQUAL(L"true", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(L"4", it->first);
    BOOST_CHECK_EQUAL(L"null", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(object_values_get_tagged) {
    ptree tree;
    prefixing_parser<char> p;
    const char* input = "{\n"
        "\"1\": 123, \"2\": \"abc\" ,\"3\": true ,\n"
        "\"4\": null\n"
    "}";
    BOOST_REQUIRE(p.parse_object(input, tree));
    BOOST_REQUIRE_EQUAL(4u, tree.size());
    BOOST_CHECK_EQUAL("o:", tree.data());
    ptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL("1", it->first);
    BOOST_CHECK_EQUAL("n:123", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("2", it->first);
    BOOST_CHECK_EQUAL("s:abc", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("3", it->first);
    BOOST_CHECK_EQUAL("b:true", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("4", it->first);
    BOOST_CHECK_EQUAL("_:null", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(nested_object) {
    ptree tree;
    standard_parser<char> p;
    const char* input = "{\"a\":{\"b\":1,\"c\":2},\"d\":3,\"e\":{\"f\":4,\"g\":5}}";
    BOOST_REQUIRE(p.parse_object(input, tree));
    BOOST_REQUIRE_EQUAL(3u, tree.size());
    ptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL("a", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("b", iit->first);
        BOOST_CHECK_EQUAL("1", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("c", iit->first);
        BOOST_CHECK_EQUAL("2", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL("d", it->first);
    BOOST_CHECK_EQUAL("3", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("e", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("f", iit->first);
        BOOST_CHECK_EQUAL("4", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("g", iit->first);
        BOOST_CHECK_EQUAL("5", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(array_in_object) {
    ptree tree;
    standard_parser<char> p;
    const char* input = "{\"a\":[1,2],\"b\":3,\"c\":[4,5]}";
    BOOST_REQUIRE(p.parse_object(input, tree));
    BOOST_REQUIRE_EQUAL(3u, tree.size());
    ptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL("a", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("1", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("2", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL("b", it->first);
    BOOST_CHECK_EQUAL("3", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("c", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("4", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("5", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(object_in_array) {
    ptree tree;
    standard_parser<char> p;
    const char* input = "[{\"a\":1,\"b\":2},3,{\"c\":4,\"d\":5}]";
    BOOST_REQUIRE(p.parse_array(input, tree));
    BOOST_REQUIRE_EQUAL(3u, tree.size());
    ptree::iterator it = tree.begin();
    BOOST_CHECK_EQUAL("", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("a", iit->first);
        BOOST_CHECK_EQUAL("1", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("b", iit->first);
        BOOST_CHECK_EQUAL("2", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL("", it->first);
    BOOST_CHECK_EQUAL("3", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("", it->first);
    {
        ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(2u, sub.size());
        ptree::iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("c", iit->first);
        BOOST_CHECK_EQUAL("4", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("d", iit->first);
        BOOST_CHECK_EQUAL("5", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

BOOST_AUTO_TEST_CASE(parser_works_with_input_iterators) {
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
    BOOST_REQUIRE_EQUAL(5u, tree.size());
    ptree::const_iterator it = tree.begin();
    BOOST_CHECK_EQUAL("1", it->first);
    BOOST_CHECK_EQUAL("123", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("2", it->first);
    BOOST_CHECK_EQUAL("abc", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("3", it->first);
    BOOST_CHECK_EQUAL("true", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("4", it->first);
    BOOST_CHECK_EQUAL("null", it->second.data());
    ++it;
    BOOST_CHECK_EQUAL("5", it->first);
    {
        const ptree& sub = it->second;
        BOOST_CHECK_EQUAL("", sub.data());
        BOOST_REQUIRE_EQUAL(3u, sub.size());
        ptree::const_iterator iit = sub.begin();
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("1", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("23", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL("", iit->first);
        BOOST_CHECK_EQUAL("456", iit->second.data());
        ++iit;
        BOOST_CHECK_EQUAL(sub.end(), iit);
    }
    ++it;
    BOOST_CHECK_EQUAL(tree.end(), it);
}

struct bad_parse_n {
    const char* json;
    const char* message_substring;
};

void parse_error_thrown_with_message_n(bad_parse_n param) {
    try {
        standard_parser<char> p;
        ptree dummy;
        p.parse_value(param.json, dummy);
        BOOST_FAIL("expected exception");
    } catch (json_parser::json_parser_error& e) {
        std::string message = e.message();
        BOOST_CHECK_MESSAGE(message.find(param.message_substring) !=
                                std::string::npos,
                            "bad error message on input '" << param.json
                                << "', need: '" << param.message_substring
                                << "' but found '" << message << "'");
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

void parse_error_thrown_with_message_w(bad_parse_w param) {
    try {
        standard_parser<wchar_t> p;
        wptree dummy;
        p.parse_value(param.json, dummy);
        BOOST_FAIL("expected exception");
    } catch (json_parser::json_parser_error& e) {
        std::string message = e.message();
        BOOST_CHECK_MESSAGE(message.find(param.message_substring) !=
                                std::string::npos,
                            "bad error message on input '" << param.json
                                << "', need: '" << param.message_substring
                                << "' but found '" << message << "'");
    }
}

const bad_parse_w errors_w[] = {
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

template <typename T, std::size_t N>
std::size_t arraysize(T(&)[N]) { return N; }

#define ARRAY_TEST_CASE(fn, a) \
    BOOST_PARAM_TEST_CASE(fn, (a), (a) + arraysize((a)))

using namespace boost::unit_test;

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
