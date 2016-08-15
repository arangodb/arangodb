#ifndef JSON_PARSER_PREFIXING_CALLBACKS_HPP
#define JSON_PARSER_PREFIXING_CALLBACKS_HPP

#include <boost/property_tree/json_parser/detail/standard_callbacks.hpp>

namespace constants
{
    template <typename Ch> const Ch* null_prefix();
    template <> inline const char* null_prefix() { return "_:"; }
    template <> inline const wchar_t* null_prefix() { return L"_:"; }

    template <typename Ch> const Ch* boolean_prefix();
    template <> inline const char* boolean_prefix() { return "b:"; }
    template <> inline const wchar_t* boolean_prefix() { return L"b:"; }

    template <typename Ch> const Ch* number_prefix();
    template <> inline const char* number_prefix() { return "n:"; }
    template <> inline const wchar_t* number_prefix() { return L"n:"; }

    template <typename Ch> const Ch* string_prefix();
    template <> inline const char* string_prefix() { return "s:"; }
    template <> inline const wchar_t* string_prefix() { return L"s:"; }

    template <typename Ch> const Ch* array_prefix();
    template <> inline const char* array_prefix() { return "a:"; }
    template <> inline const wchar_t* array_prefix() { return L"a:"; }

    template <typename Ch> const Ch* object_prefix();
    template <> inline const char* object_prefix() { return "o:"; }
    template <> inline const wchar_t* object_prefix() { return L"o:"; }
}

template <typename Ptree>
struct prefixing_callbacks
        : boost::property_tree::json_parser::detail::standard_callbacks<Ptree> {
    typedef boost::property_tree::json_parser::detail::standard_callbacks<Ptree>
        base;
    typedef typename base::string string;
    typedef typename base::char_type char_type;

    void on_null() {
        base::on_null();
        this->current_value().insert(0, constants::null_prefix<char_type>());
    }

    void on_boolean(bool b) {
        base::on_boolean(b);
        this->current_value().insert(0, constants::boolean_prefix<char_type>());
    }

    template <typename Range>
    void on_number(Range code_units) {
        base::on_number(code_units);
        this->current_value().insert(0, constants::number_prefix<char_type>());
    }
    void on_begin_number() {
        base::on_begin_number();
        this->current_value() = constants::number_prefix<char_type>();
    }

    void on_begin_string() {
        base::on_begin_string();
        if (!this->is_key()) {
            this->current_value() = constants::string_prefix<char_type>();
        }
    }

    void on_begin_array() {
        base::on_begin_array();
        this->current_value() = constants::array_prefix<char_type>();
    }

    void on_begin_object() {
        base::on_begin_object();
        this->current_value() = constants::object_prefix<char_type>();
    }
};

#endif
