//
// Created by lars on 2019-12-04.
//

#ifndef VELOCYPACK_ERRORS_H
#define VELOCYPACK_ERRORS_H
#include <velocypack/Dumper.h>
#include <velocypack/Sink.h>
#include <utility>
#include <variant>
#include "gadgets.h"

struct error {
  using field_name = std::string;
  using index = std::size_t;

  struct key_value_annotation {
    std::string key, value;
    key_value_annotation(std::string key, std::string value)
        : key(std::move(key)), value(std::move(value)) {}
  };

  struct hint {
    std::string msg;
    explicit hint(std::string msg) : msg(std::move(msg)) {}
  };

  using access_type = std::variant<field_name, index, key_value_annotation, hint>;

  error(error&&) noexcept = default;
  error(error const&) = default;

  error& operator=(error&&) = default;
  error& operator=(error const&) = default;

  error() noexcept = default;
  explicit error(std::string message)
      : backtrace(), message(std::move(message)) {}

  std::vector<access_type> backtrace;
  std::string message;

  [[nodiscard]] bool is_error() const { return !message.empty(); }
  operator bool() const { return is_error(); }

  error& trace(std::string field) & {
    backtrace.emplace_back(std::in_place_type<field_name>, std::move(field));
    return *this;
  }

  error& trace(std::size_t index) & {
    backtrace.emplace_back(index);
    return *this;
  }

  error& annotate(std::string key, std::string value) & {
    backtrace.emplace_back(std::in_place_type<key_value_annotation>,
                           std::move(key), std::move(value));
    return *this;
  }

  error& wrap(const std::string& wrap) & {
    backtrace.emplace_back(std::in_place_type<hint>, wrap);
    return *this;
  }

  error trace(std::string field) && {
    backtrace.emplace_back(std::in_place_type<field_name>, std::move(field));
    return std::move(*this);
  }

  error trace(std::size_t index) && {
    backtrace.emplace_back(index);
    return std::move(*this);
  }

  error annotate(std::string key, std::string value) && {
    backtrace.emplace_back(std::in_place_type<key_value_annotation>,
                           std::move(key), std::move(value));
    return std::move(*this);
  }

  error wrap(const std::string& wrap) && {
    backtrace.emplace_back(std::in_place_type<hint>, wrap);
    return std::move(*this);
  }

  operator std::string() const { return as_string(); }
  [[nodiscard]] std::string as_string(bool detailed = false) const {
    using deserializer::detail::gadgets::visitor;
    std::string result;
    bool was_terminated = false;
    for (auto i = backtrace.crbegin(); i != backtrace.crend(); i++) {
      std::visit(visitor{[&](field_name const& field) {
                           if (was_terminated) {
                             result += " at ";
                             was_terminated = false;
                           }
                           if (is_identifier(field)) {
                             result += '.';
                             result += field;
                           } else {
                             result += '[';
                             result += dump_json_string(field);
                             result += ']';
                           }
                         },
                         [&](index index) {
                           if (was_terminated) {
                             result += " at ";
                             was_terminated = false;
                           }
                           result += '[';
                           result += std::to_string(index);
                           result += ']';
                         },
                         [&](key_value_annotation const& ann) {
                           result += " with ";
                           result += dump_json_string(ann.key);
                           result += " == ";
                           result += dump_json_string(ann.value);
                           result += ':';
                           was_terminated = true;
                         },
                         [](auto) {}},
                 *i);
    }
    if (backtrace.empty()) {
      result += "(top-level)";
    }

    if (!was_terminated) {
      result += ':';
    }
    result += ' ';
    result += message;
    return result;
  }

 private:
  static std::string dump_json_string(std::string const& string) {
    using arangodb::velocypack::Dumper;
    using arangodb::velocypack::Sink;

    std::string result;
    arangodb::velocypack::StringSink sink(&result);
    Dumper(&sink).appendString(string);
    return result;
  }

  static bool is_identifier(std::string const& name) {
    bool first = true;
    for (char c : name) {
      bool is_alpha = ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') ||
                      c == '$' || c == '_';
      bool is_num = ('0' <= c && c <= '9');

      if (!is_alpha && (first || !is_num)) {
        return false;
      }

      first = false;
    }
    return true;
  }
};

using deserialize_error = error;
/*
struct deserialize_error {
  explicit deserialize_error() noexcept = default;
  explicit deserialize_error(std::string msg) : msg(std::move(msg)) {}

  [[nodiscard]] std::string const& what() const { return msg; }
  [[nodiscard]] deserialize_error wrap(std::string const& wrap) const {
    return deserialize_error(wrap + ": " + msg);
  }

 private:
  std::string msg;
};*/

#endif  // VELOCYPACK_ERRORS_H
