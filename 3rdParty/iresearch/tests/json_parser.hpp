//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_JSON_PARSER_TESTS
#define IRESEARCH_JSON_PARSER_TESTS

#include <iostream>
#include <vector>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

NS_BEGIN(tests)
NS_BEGIN(json)

using boost::property_tree::ptree;

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to be used with boost::property_tree::basic_ptree as its
///        value_type
////////////////////////////////////////////////////////////////////////////////
#if BOOST_VERSION < 105900
  struct json_value {
    bool quoted;
    std::string value;
    json_value() {}
    json_value(std::string& v_value, bool v_quoted):
      quoted(v_quoted), value(std::move(v_value)) {}
    json_value(const json_value& other):
      quoted(other.quoted), value(other.value) {}
    json_value(json_value&& other) NOEXCEPT
      : quoted(other.quoted), value(std::move(other.value)) {}
    json_value& operator=(json_value&& other) NOEXCEPT {
      quoted = other.quoted;
      value = std::move(other.value);
      return *this;
    }
  };
#else // below definition required for boost v1.59.0+
  struct json_value {
    typedef char value_type; // must exactly match json_key::value_type
    bool quoted;
    std::string value;

    json_value& operator=(const value_type* v_value) {
      // from v1.59.0+ boost::property_tree::json_parser::detail::constants
      static const std::string v_false("false");
      static const std::string v_null("null");
      static const std::string v_true("true");

      if (v_false == v_value) {
        value = "false"; // match string values for boost < v1.59.0
      } else if (v_null == v_value) {
        value = "null"; // match string values for boost < v1.59.0
      } else if (v_true == v_value) {
        value = "true"; // match string values for boost < v1.59.0
      } else {
        throw std::string(v_value);
      }

      quoted = false;

      return *this;
    }
    json_value& operator+=(const value_type& v) { value += v; return *this; }
    void clear() { quoted = false; value.clear(); }
  };
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to be used with boost::property_tree::basic_ptree as key_type
///        Note: Boost JSON parser uses key_type both as a key holder and as a
///              factory method for boost::property_tree::basic_ptree value_type
///        Note2: Boost JSON parser assumes key_type can be used interchangeably
///               with std::string
////////////////////////////////////////////////////////////////////////////////
#if BOOST_VERSION < 105900
  class json_key {
   public:
    typedef char value_type; // must be a type initializable from char
    json_key(): quoted_(true) {} // constructor used by parser for string values
    template <class InputIterator>
    json_key(InputIterator first, InputIterator last):
      quoted_(false), value_(first, last) {
    } // constructor used by parser for literal values

    bool operator<(const json_key& other) const { return value_ < other.value_; }
    json_key& operator+=(const value_type& v) { value_ += v; return *this; }
    operator const std::string&() const { return value_; }
    operator json_value() {
      // operator used by Boost JSON parser as a factory method for value_type
      json_value value;

      value.quoted = quoted_;
      value.value = std::move(value_);

      return value;
    }

    const char* c_str() const { return value_.c_str(); }
    void clear() { value_.clear(); }
    bool empty() const { return value_.empty(); }
    void swap(json_key& v) { value_.swap(v.value_); }
   private:
     bool quoted_;
     std::string value_;
  };
#else // below definition required for boost v1.59.0+
  class json_key {
   public:
    bool operator<(const json_key& other) const { return value_ < other.value_; }
    operator const std::string&() const { return value_; }
    json_key(const json_value& value): value_(value.value) {}
    bool empty() const { return value_.empty(); }
   private:
    std::string value_;
  };
#endif

typedef boost::property_tree::basic_ptree<json_key, json_value> json_tree;

struct parser_context {
  size_t level;
  std::vector<std::string> path;
};

struct visitor {
  /* called at the beginning of a document */
  void begin_doc();

  /* called at the beginning of an array */
  void begin_array( const parser_context& ctx );
  
  /* called at the beginning of an object */
  void begin_object( const parser_context& ctx );
  
  /* called at value entry */
  void node( const parser_context& ctx,
             const ptree::value_type& name );

  /* called at the end of an array  */
  void end_array( const parser_context& ctx );
  
  /* called at the end of an object */
  void end_object( const parser_context& ctx );
    
  /* called at the end of document */
  void end_doc();
};

template<typename JsonVisitor, typename JsonTreeItr>
void parse_json(JsonTreeItr begin, JsonTreeItr end, JsonVisitor& v, parser_context& ctx) {
  for ( ; begin != end; ++begin ) {
    if ( begin->second.empty() ) {
      v.node( ctx, *begin );
      continue;
    }

    ++ctx.level;

    /* we are in array */
    const bool in_array = begin->first.empty();
    /* next element is array (arrays have empty name) */
    const bool next_is_array = begin->second.begin()->first.empty();

    if ( !in_array ) {
      ctx.path.push_back( begin->first ); 
    }    

    if ( next_is_array ) {
      v.begin_array( ctx ); 
    } else { 
      v.begin_object( ctx ); 
    }

    parse_json( begin->second.begin(), begin->second.end(), v, ctx );

    if ( next_is_array ) {
      v.end_array( ctx );
    } else { 
      v.end_object( ctx ); 
    }

    if ( !in_array ) {
      ctx.path.pop_back(); 
    }

    --ctx.level;
  }
}

template<typename JsonVisitor, typename JsonTree>
void parse_json(const JsonTree& json, JsonVisitor& v) {
  parser_context ctx{ 0 };
  v.begin_doc();
  parse_json( json.begin(), json.end(), v, ctx );
  v.end_doc();
}

NS_END // json
NS_END // tests

// required for 'json_tree' type to compile
namespace boost {
  namespace property_tree {
    template <>
    struct path_of<tests::json::json_key>
    {
      typedef std::string type;
    };
  }
}

// defined after boost::property_tree::path_of<tests::json::json_key>
NS_BEGIN(tests)
NS_BEGIN(json)

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to be used with boost::property_tree::read_json_internal for
///        boost v1.59.0+ as its callback
////////////////////////////////////////////////////////////////////////////////
#if BOOST_VERSION < 105900
  // NOOP, not requred, use boost::property_tree::json_parser::read_json(...)
#else
  class json_callbacks {
   public:
    typedef json_tree::data_type::value_type char_type;
    class callbacks:
      public boost::property_tree::json_parser::detail::standard_callbacks<json_tree> {
      typedef boost::property_tree::json_parser::detail::standard_callbacks<json_tree> parent_t;
      public:
      json_tree::data_type& current_value() { return parent_t::current_value(); } // allow public visibility
    };
    callbacks callbacks;
    void on_begin_array() { callbacks.on_begin_array(); callbacks.current_value().quoted = false; }
    void on_begin_number() { callbacks.on_begin_number(); callbacks.current_value().quoted = false; }
    void on_begin_object() { callbacks.on_begin_object(); callbacks.current_value().quoted = false; }
    void on_begin_string() { callbacks.on_begin_string(); callbacks.current_value().quoted = true; }
    void on_boolean(bool b) { callbacks.on_boolean(b); callbacks.current_value().quoted = false; }
    void on_code_unit(char_type c) { callbacks.on_code_unit(c); }
    void on_digit(char_type d) { callbacks.on_digit(d); }
    void on_end_array() { callbacks.on_end_array(); }
    void on_end_number() { callbacks.on_end_number(); }
    void on_end_object() { callbacks.on_end_object(); }
    void on_end_string() { callbacks.on_end_string(); }
    void on_null() { callbacks.on_null(); }
    json_tree& output() { return callbacks.output(); }
  };
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief a class to be used with boost::property_tree::read_json_internal for
///        boost v1.59.0+ as a wrapper for an std::stream iterator to mask jSON
///        comments since boost::property_tree::json_parser in boost v1.59.0+
///        does not support comments
////////////////////////////////////////////////////////////////////////////////
#if BOOST_VERSION < 105900
  // NOOP, not requred, use boost::property_tree::json_parser::read_json(...)
#else
  template<typename Iterator>
  class json_comment_masking_iterator {
   public:
    typedef typename Iterator::difference_type difference_type;
    typedef typename Iterator::iterator_category iterator_category;
    typedef typename Iterator::pointer pointer;
    typedef typename Iterator::reference reference;
    typedef typename Iterator::value_type value_type;
    json_comment_masking_iterator() {}
    json_comment_masking_iterator(Iterator iterator): iterator_(iterator) {
      skip_comments();
    }
    bool operator==(const Iterator& other) const { return iterator_ == other; }
    bool operator!=(const Iterator& other) const { return iterator_ != other; }
    reference operator*() const { return *iterator_; }
    json_comment_masking_iterator& operator++() {
      ++iterator_;

      if (!quoted_) {
        skip_comments();
      }

      if (iterator_ != end_ && *iterator_ == '"') {
        quoted_ = !quoted_;
      }

      return *this;
    }

   private:
    Iterator end_{};
    Iterator iterator_{};
    bool quoted_{};

    void skip_comments() {
      while (iterator_ != end_ && *iterator_ == '/') {
        Iterator orig_itr = iterator_;

        if (++iterator_ == end_) {
          // not a comment, restore to previous iterator position
          iterator_ = orig_itr;

          return;
        }

        // start of single line comment
        if (*iterator_ == '/') {
          do {
            if (++iterator_ == end_) {
              return;
            }
          } while (*iterator_ != '\n' && *iterator_ != '\r');

          return; // end of comment
        }

        // start of multiline comment
        if (*iterator_ == '*') {
          if (++iterator_ == end_) {
            return;
          }

          // skip all input until closing "*/"
          do {
            while (*iterator_ != '*') {
              if (++iterator_ == end_) {
                return;
              }
            };

            if (++iterator_ == end_) {
              return;
            }
          } while (*iterator_ != '/');

          ++iterator_;

          continue; // check for start of next comment right after this one
        }

        // not a comment, restore to previous iterator position
        iterator_ = orig_itr;

        return; // not a comment
      }
    }
  };
#endif

NS_END // json
NS_END // tests

#endif