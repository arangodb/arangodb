////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <sstream>
#include "tests_shared.hpp"
#include "analysis/ngram_token_stream.hpp"
#include "utils/locale_utils.hpp"
#include <utf8.h>

#ifndef IRESEARCH_DLL

TEST(ngram_token_stream_test, consts) {
  static_assert("ngram" == irs::type<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>::name());
  static_assert("ngram" == irs::type<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8>>::name());
}

TEST(ngram_token_stream_test, construct) {
  // load jSON object
  {
    auto stream = irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"min\":1, \"max\":3, \"preserveOriginal\":true}");
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>::id(), stream->type());

    auto& impl = dynamic_cast<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>&>(*stream);
    ASSERT_EQ(1, impl.min_gram());
    ASSERT_EQ(3, impl.max_gram());
    ASSERT_EQ(true, impl.preserve_original());
  }

  // load jSON object
  {
    auto stream = irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"min\":0, \"max\":1, \"preserveOriginal\":false, \"invalidProperty\":true }");
    ASSERT_NE(nullptr, stream);

    auto& impl = dynamic_cast<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>&>(*stream);
    ASSERT_EQ(1, impl.min_gram());
    ASSERT_EQ(1, impl.max_gram());
    ASSERT_EQ(false, impl.preserve_original());
  }

  // load jSON object
  {
    auto stream = irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"min\":2, \"max\":1, \"preserveOriginal\":false }");
    ASSERT_NE(nullptr, stream);

    auto& impl = dynamic_cast<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>&>(*stream);
    ASSERT_EQ(2, impl.min_gram());
    ASSERT_EQ(2, impl.max_gram());
    ASSERT_EQ(false, impl.preserve_original());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"locale\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"min\":\"1\", \"max\":3, \"preserveOriginal\":true}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"min\":1, \"max\":\"3\", \"preserveOriginal\":true}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"min\":1, \"max\":3, \"preserveOriginal\":\"true\"}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{\"min\":1, \"max\":3, \"preserveOriginal\":\"true\",\"streamType\":\"unknown\"}"));
  }

  // 2-gram
  {
    auto stream = irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>::make(irs::analysis::ngram_token_stream_base::Options(2, 2, true));
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>::id(), stream->type());

    auto impl = std::dynamic_pointer_cast<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>(stream);
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ(2, impl->min_gram());
    ASSERT_EQ(2, impl->max_gram());
    ASSERT_EQ(true, impl->preserve_original());

    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_TRUE(term);
    ASSERT_TRUE(term->value.null());

    auto* increment = irs::get<irs::increment>(*stream);
    ASSERT_TRUE(increment);
    ASSERT_EQ(1, increment->value);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_TRUE(offset);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(0, offset->end);
  }

  // 0 == min_gram
  {
    auto stream = irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>::make(irs::analysis::ngram_token_stream_base::Options(0, 2, true));
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>::id(), stream->type());

    auto impl = std::dynamic_pointer_cast<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>(stream);
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ(1, impl->min_gram());
    ASSERT_EQ(2, impl->max_gram());
    ASSERT_EQ(true, impl->preserve_original());

    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_TRUE(term);
    ASSERT_TRUE(term->value.null());

    auto* increment = irs::get<irs::increment>(*stream);
    ASSERT_TRUE(increment);
    ASSERT_EQ(1, increment->value);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_TRUE(offset);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(0, offset->end);
  }

  // min_gram > max_gram
  {
    auto stream = irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>::make(irs::analysis::ngram_token_stream_base::Options(std::numeric_limits<size_t>::max(), 2, true));
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::type<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>::id(), stream->type());

    auto impl = std::dynamic_pointer_cast<irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>>(stream);
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ(std::numeric_limits<size_t>::max(), impl->min_gram());
    ASSERT_EQ(std::numeric_limits<size_t>::max(), impl->max_gram());
    ASSERT_EQ(true, impl->preserve_original());

    auto* term = irs::get<irs::term_attribute>(*stream);
    ASSERT_TRUE(term);
    ASSERT_TRUE(term->value.null());

    auto* increment = irs::get<irs::increment>(*stream);
    ASSERT_TRUE(increment);
    ASSERT_EQ(1, increment->value);

    auto* offset = irs::get<irs::offset>(*stream);
    ASSERT_TRUE(offset);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(0, offset->end);
  }
}

TEST(ngram_token_stream_test, next_utf8) {
  struct utf8token {
    utf8token(const irs::string_ref& value, size_t start, size_t end) noexcept
      : value(value.c_str(), value.size()), start(start), end(end),
        start_marker(irs::string_ref::EMPTY),
        end_marker(irs::string_ref::EMPTY) {}
    utf8token(const irs::string_ref& value, size_t start, size_t end, irs::string_ref sm, irs::string_ref em) noexcept
      : value(value.c_str(), value.size()), start(start), end(end), start_marker(sm), end_marker(em) {}

    irs::string_ref start_marker;
    irs::string_ref end_marker;
    irs::string_ref value;
    size_t start;
    size_t end;
  };

  auto assert_utf8tokens = [](
      const std::vector<utf8token>& expected,
      const irs::string_ref& data,
      irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8>& stream) {
    ASSERT_TRUE(stream.reset(data));

    auto* value = irs::get<irs::term_attribute>(stream);
    ASSERT_TRUE(value);

    auto* offset = irs::get<irs::offset>(stream);
    ASSERT_TRUE(offset);
    auto* inc = irs::get<irs::increment>(stream);
    auto expected_token = expected.begin();
    uint32_t pos = iresearch::integer_traits<uint32_t>::const_max;
    while (stream.next()) {
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(expected_token->value), value->value);
      ASSERT_EQ(expected_token->start, offset->start);
      ASSERT_EQ(expected_token->end, offset->end);
      pos += inc->value;
      auto start = reinterpret_cast<const irs::byte_type*>(data.begin());
      utf8::unchecked::advance(start, pos);
      const auto size = value->value.size() - expected_token->start_marker.size() - expected_token->end_marker.size();
      ASSERT_GT(size, 0);
      irs::bstring bs;
      if (!expected_token->start_marker.empty()) {
        bs.append(reinterpret_cast<const irs::byte_type*>(expected_token->start_marker.c_str()), expected_token->start_marker.size());
      }
      bs.append(start, size);
      if (!expected_token->end_marker.empty()) {
        bs.append(reinterpret_cast<const irs::byte_type*>(expected_token->end_marker.c_str()), expected_token->end_marker.size());
      }

      ASSERT_EQ(bs, value->value);
      ++expected_token;
    }
    ASSERT_EQ(expected_token, expected.end());
    ASSERT_FALSE(stream.next());
  };

  auto locale = irs::locale_utils::locale("C.UTF-8");

  {
    SCOPED_TRACE("1-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(1, 1, false,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected {
      { "a", 0, 1 },
      { "\xc2\xa2", 1, 3 },
      { "b", 3, 4 },
      { "\xc2\xa3", 4, 6 },
      { "c", 6, 7 },
      { "\xc2\xa4", 7, 9 },
      { "d", 9, 10 },
      { "\xc2\xa5", 10, 12 }
    };
    assert_utf8tokens(expected, data, stream);

    // let`s break utf-8. Cut the last byte of last 2-byte symbol
    data.resize(data.size() - 1);
    const std::vector<utf8token> expected2 {
      { "a", 0, 1 },
      { "\xc2\xa2", 1, 3 },
      { "b", 3, 4 },
      { "\xc2\xa3", 4, 6 },
      { "c", 6, 7 },
      { "\xc2\xa4", 7, 9 },
      { "d", 9, 10 },
      { "\xc2", 10, 11 }
    };
    assert_utf8tokens(expected2, data, stream);
  }

  {
    SCOPED_TRACE("2-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(2, 2, false,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected {
      { "a\xc2\xa2", 0, 3 },
      { "\xc2\xa2""b", 1, 4 },
      { "b""\xc2\xa3", 3, 6 },
      { "\xc2\xa3""c", 4, 7 },
      { "c""\xc2\xa4", 6, 9 },
      { "\xc2\xa4""d", 7, 10 },
      { "d""\xc2\xa5", 9, 12 }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE("1-2-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(1, 2, false,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected{
      { "a", 0, 1 },
      { "a\xc2\xa2", 0, 3 },
      { "\xc2\xa2", 1, 3 },
      { "\xc2\xa2""b", 1, 4 },
      { "b", 3, 4 },
      { "b""\xc2\xa3", 3, 6 },
      { "\xc2\xa3", 4, 6 },
      { "\xc2\xa3""c", 4, 7 },
      { "c", 6, 7 },
      { "c""\xc2\xa4", 6, 9 },
      { "\xc2\xa4", 7, 9 },
      { "\xc2\xa4""d", 7, 10 },
      { "d", 9, 10 },
      { "d""\xc2\xa5", 9, 12 },
      { "\xc2\xa5", 10, 12 }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE("5-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
        irs::analysis::ngram_token_stream_base::Options(5, 5, false,
                                                     irs::analysis::ngram_token_stream_base::InputType::UTF8,
                                                     irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));
    std::wstring sDataUCS2 = L"\u00C0\u00C1\u00C2\u00C3\u00C4";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
    const std::vector<utf8token> expected{
      { "\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84", 0, 10 }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE("5-gram with markers");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(5, 5, false,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa2"), 2), irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa1"), 2)));
    std::wstring sDataUCS2 = L"\u00C0\u00C1\u00C2\u00C3\u00C4";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
    const std::vector<utf8token> expected{
      { "\xc2\xa2\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84", 0, 10, "\xc2\xa2", irs::string_ref::EMPTY },
      { "\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84\xc2\xa1", 0, 10, irs::string_ref::EMPTY, "\xc2\xa1" }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE("5-gram preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
        irs::analysis::ngram_token_stream_base::Options(5, 5, true,
                                                     irs::analysis::ngram_token_stream_base::InputType::UTF8,
                                                     irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));
    std::wstring sDataUCS2 = L"\u00C0\u00C1\u00C2\u00C3\u00C4";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
    const std::vector<utf8token> expected{
      { "\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84", 0, 10}
    };
    assert_utf8tokens(expected, data, stream);
  }


  {
    SCOPED_TRACE("6-gram preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(6, 6, true,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));
    std::wstring sDataUCS2 = L"\u00C0\u00C1\u00C2\u00C3\u00C4";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
    const std::vector<utf8token> expected{
      { "\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84", 0, 10 }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE("6-gram no output");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(6, 6, false,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));
    std::wstring sDataUCS2 = L"\u00C0\u00C1\u00C2\u00C3\u00C4";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
    ASSERT_TRUE(stream.reset(data));
    ASSERT_FALSE(stream.next());
  }

  {
    SCOPED_TRACE("1-2 gram no-preserve-original start-marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(1, 2, false,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa1"), 2), irs::bytes_ref::EMPTY));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected{
      { "\xc2\xa1""a", 0, 1, "\xc2\xa1", irs::string_ref::EMPTY },
      { "\xc2\xa1""a""\xc2\xa2", 0, 3, "\xc2\xa1", irs::string_ref::EMPTY },
      { "\xc2\xa2", 1, 3 },
      { "\xc2\xa2""b", 1, 4 },
      { "b", 3, 4 },
      { "b""\xc2\xa3", 3, 6 },
      { "\xc2\xa3", 4, 6 },
      { "\xc2\xa3""c", 4, 7 },
      { "c", 6, 7 },
      { "c""\xc2\xa4", 6, 9 },
      { "\xc2\xa4", 7, 9 },
      { "\xc2\xa4""d", 7, 10 },
      { "d", 9, 10 },
      { "d""\xc2\xa5", 9, 12 },
      { "\xc2\xa5", 10, 12 }
    };
    assert_utf8tokens(expected, data, stream);
  }
  {
    SCOPED_TRACE("1-2 gram preserve-original start-marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(1, 2, true,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref(reinterpret_cast <const irs::byte_type*>("\xc2\xa1"), 2), irs::bytes_ref::EMPTY));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected{
      { "\xc2\xa1""a", 0, 1, "\xc2\xa1", irs::string_ref::EMPTY },
      { "\xc2\xa1""a""\xc2\xa2", 0, 3, "\xc2\xa1", irs::string_ref::EMPTY },
      { "\xc2\xa1""a""\xc2\xa2""b""\xc2\xa3""c""\xc2\xa4""d""\xc2\xa5", 0, 12, "\xc2\xa1", irs::string_ref::EMPTY},
      { "\xc2\xa2", 1, 3 },
      { "\xc2\xa2""b", 1, 4 },
      { "b", 3, 4 },
      { "b""\xc2\xa3", 3, 6 },
      { "\xc2\xa3", 4, 6 },
      { "\xc2\xa3""c", 4, 7 },
      { "c", 6, 7 },
      { "c""\xc2\xa4", 6, 9 },
      { "\xc2\xa4", 7, 9 },
      { "\xc2\xa4""d", 7, 10 },
      { "d", 9, 10 },
      { "d""\xc2\xa5", 9, 12 },
      { "\xc2\xa5", 10, 12 }
    };
    assert_utf8tokens(expected, data, stream);
  }
  {
    SCOPED_TRACE("2-3 gram preserve-original end-marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(2, 3, true,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref::EMPTY, irs::bytes_ref(reinterpret_cast <const irs::byte_type*>("\xc2\xa1"), 2)));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected{
      { "a""\xc2\xa2", 0, 3},
      { "a""\xc2\xa2""b", 0, 4 },
      { "a""\xc2\xa2""b""\xc2\xa3""c""\xc2\xa4""d""\xc2\xa5""\xc2\xa1", 0, 12, irs::string_ref::EMPTY, "\xc2\xa1"},
      { "\xc2\xa2""b", 1, 4 },
      { "\xc2\xa2""b""\xc2\xa3", 1, 6 },
      { "b""\xc2\xa3", 3, 6 },
      { "b""\xc2\xa3""c", 3, 7 },
      { "\xc2\xa3""c", 4, 7 },
      { "\xc2\xa3""c""\xc2\xa4", 4, 9 },
      { "c""\xc2\xa4", 6, 9 },
      { "c""\xc2\xa4""d", 6, 10},
      { "\xc2\xa4""d", 7, 10 },
      { "\xc2\xa4""d""\xc2\xa5""\xc2\xa1", 7, 12, irs::string_ref::EMPTY, "\xc2\xa1"  },
      { "d""\xc2\xa5""\xc2\xa1", 9, 12, irs::string_ref::EMPTY, "\xc2\xa1"  }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE(" 1-3 gram preserve-original end-marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(1, 3, true,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref::EMPTY, irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa1"), 2)));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected{
      { "a", 0, 1},
      { "a""\xc2\xa2", 0, 3 },
      { "a""\xc2\xa2""b", 0, 4 },
      { "a""\xc2\xa2""b""\xc2\xa3""c""\xc2\xa4""d""\xc2\xa5""\xc2\xa1", 0, 12, irs::string_ref::EMPTY, "\xc2\xa1"},
      { "\xc2\xa2", 1, 3 },
      { "\xc2\xa2""b", 1, 4 },
      { "\xc2\xa2""b""\xc2\xa3", 1, 6 },
      { "b", 3, 4 },
      { "b""\xc2\xa3", 3, 6 },
      { "b""\xc2\xa3""c", 3, 7 },
      { "\xc2\xa3", 4, 6 },
      { "\xc2\xa3""c", 4, 7 },
      { "\xc2\xa3""c""\xc2\xa4", 4, 9 },
      { "c", 6, 7 },
      { "c""\xc2\xa4", 6, 9 },
      { "c""\xc2\xa4""d", 6, 10},
      { "\xc2\xa4", 7, 9 },
      { "\xc2\xa4""d", 7, 10 },
      { "\xc2\xa4""d""\xc2\xa5""\xc2\xa1", 7, 12, irs::string_ref::EMPTY, "\xc2\xa1" },
      { "d", 9, 10 },
      { "d""\xc2\xa5""\xc2\xa1", 9, 12, irs::string_ref::EMPTY, "\xc2\xa1" },
      { "\xc2\xa5""\xc2\xa1", 10, 12, irs::string_ref::EMPTY, "\xc2\xa1" }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE("1-3 gram preserve-original start-marker end-marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(1, 3, true,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa2"), 2), irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa1"), 2)));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected{
      { "\xc2\xa2""a", 0, 1, "\xc2\xa2", irs::string_ref::EMPTY},
      { "\xc2\xa2""a""\xc2\xa2", 0, 3, "\xc2\xa2", irs::string_ref::EMPTY },
      { "\xc2\xa2""a""\xc2\xa2""b", 0, 4, "\xc2\xa2", irs::string_ref::EMPTY },
      { "\xc2\xa2""a""\xc2\xa2""b""\xc2\xa3""c""\xc2\xa4""d""\xc2\xa5", 0, 12, "\xc2\xa2", irs::string_ref::EMPTY},
      { "a""\xc2\xa2""b""\xc2\xa3""c""\xc2\xa4""d""\xc2\xa5""\xc2\xa1", 0, 12, irs::string_ref::EMPTY, "\xc2\xa1"},
      { "\xc2\xa2", 1, 3 },
      { "\xc2\xa2""b", 1, 4 },
      { "\xc2\xa2""b""\xc2\xa3", 1, 6 },
      { "b", 3, 4 },
      { "b""\xc2\xa3", 3, 6 },
      { "b""\xc2\xa3""c", 3, 7 },
      { "\xc2\xa3", 4, 6 },
      { "\xc2\xa3""c", 4, 7 },
      { "\xc2\xa3""c""\xc2\xa4", 4, 9 },
      { "c", 6, 7 },
      { "c""\xc2\xa4", 6, 9 },
      { "c""\xc2\xa4""d", 6, 10},
      { "\xc2\xa4", 7, 9 },
      { "\xc2\xa4""d", 7, 10 },
      { "\xc2\xa4""d""\xc2\xa5""\xc2\xa1", 7, 12, irs::string_ref::EMPTY, "\xc2\xa1" },
      { "d", 9, 10 },
      { "d""\xc2\xa5""\xc2\xa1", 9, 12, irs::string_ref::EMPTY, "\xc2\xa1" },
      { "\xc2\xa5""\xc2\xa1", 10, 12, irs::string_ref::EMPTY, "\xc2\xa1" }
    };
    assert_utf8tokens(expected, data, stream);
  }

  {
    SCOPED_TRACE("1-3 gram no-preserve-original start-marker end-marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
      irs::analysis::ngram_token_stream_base::Options(1, 3, false,
        irs::analysis::ngram_token_stream_base::InputType::UTF8,
        irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa2"), 2), irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa1"), 2)));

    std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));

    const std::vector<utf8token> expected{
      { "\xc2\xa2""a", 0, 1, "\xc2\xa2", irs::string_ref::EMPTY},
      { "\xc2\xa2""a""\xc2\xa2", 0, 3, "\xc2\xa2", irs::string_ref::EMPTY },
      { "\xc2\xa2""a""\xc2\xa2""b", 0, 4, "\xc2\xa2", irs::string_ref::EMPTY },
      { "\xc2\xa2", 1, 3 },
      { "\xc2\xa2""b", 1, 4 },
      { "\xc2\xa2""b""\xc2\xa3", 1, 6 },
      { "b", 3, 4 },
      { "b""\xc2\xa3", 3, 6 },
      { "b""\xc2\xa3""c", 3, 7 },
      { "\xc2\xa3", 4, 6 },
      { "\xc2\xa3""c", 4, 7 },
      { "\xc2\xa3""c""\xc2\xa4", 4, 9 },
      { "c", 6, 7 },
      { "c""\xc2\xa4", 6, 9 },
      { "c""\xc2\xa4""d", 6, 10},
      { "\xc2\xa4", 7, 9 },
      { "\xc2\xa4""d", 7, 10 },
      { "\xc2\xa4""d""\xc2\xa5""\xc2\xa1", 7, 12, irs::string_ref::EMPTY, "\xc2\xa1" },
      { "d", 9, 10 },
      { "d""\xc2\xa5""\xc2\xa1", 9, 12, irs::string_ref::EMPTY, "\xc2\xa1" },
      { "\xc2\xa5""\xc2\xa1", 10, 12, irs::string_ref::EMPTY, "\xc2\xa1" }
    };
    assert_utf8tokens(expected, data, stream);
  }
}


TEST(ngram_token_stream_test, reset_too_big) {
  irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 1, false));

  const irs::string_ref input(
    reinterpret_cast<const char*>(&stream),
    size_t(std::numeric_limits<uint32_t>::max()) + 1);

  ASSERT_FALSE(stream.reset(input));

  auto* term = irs::get<irs::term_attribute>(stream);
  ASSERT_TRUE(term);
  ASSERT_TRUE(term->value.null());

  auto* increment = irs::get<irs::increment>(stream);
  ASSERT_TRUE(increment);
  ASSERT_EQ(1, increment->value);

  auto* offset = irs::get<irs::offset>(stream);
  ASSERT_TRUE(offset);
  ASSERT_EQ(0, offset->start);
  ASSERT_EQ(0, offset->end);
}

TEST(ngram_token_stream_test, next) {
  struct token {
    token(const irs::string_ref& value, size_t start, size_t end) noexcept
      : value(value), start(start), end(end) ,
        start_marker(irs::string_ref::EMPTY),
        end_marker(irs::string_ref::EMPTY) {}

    token(const irs::string_ref& value, size_t start, size_t end, irs::string_ref sm, irs::string_ref em) noexcept
      : value(value.c_str(), value.size()), start(start), end(end), start_marker(sm), end_marker(em) {}

    irs::string_ref start_marker;
    irs::string_ref end_marker;

    irs::string_ref value;
    size_t start;
    size_t end;
  };

  auto assert_tokens = [](
      const std::vector<token>& expected,
      const irs::string_ref& data,
      irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>& stream) {
    ASSERT_TRUE(stream.reset(data));

    auto* value = irs::get<irs::term_attribute>(stream);
    ASSERT_TRUE(value);

    auto* offset = irs::get<irs::offset>(stream);
    ASSERT_TRUE(offset);
    auto* inc = irs::get<irs::increment>(stream);
    auto expected_token = expected.begin();
    uint32_t pos = iresearch::integer_traits<uint32_t>::const_max;
    while (stream.next()) {
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(expected_token->value), value->value);
      ASSERT_EQ(expected_token->start, offset->start);
      ASSERT_EQ(expected_token->end, offset->end);
      pos += inc->value;
      const auto size = value->value.size() - expected_token->start_marker.size() - expected_token->end_marker.size();
      ASSERT_GT(size, 0);
      irs::bstring bs;
      if (!expected_token->start_marker.empty()) {
        bs.append(reinterpret_cast<const irs::byte_type*>(expected_token->start_marker.c_str()), expected_token->start_marker.size());
      }
      bs.append(reinterpret_cast<const irs::byte_type*>(data.c_str()) + pos, size);
      if (!expected_token->end_marker.empty()) {
        bs.append(reinterpret_cast<const irs::byte_type*>(expected_token->end_marker.c_str()), expected_token->end_marker.size());
      }
      ASSERT_EQ(bs, value->value);
      ++expected_token;
    }
    ASSERT_EQ(expected_token, expected.end());
    ASSERT_FALSE(stream.next());
  };

  {
    SCOPED_TRACE("1-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 1, false));

    const std::vector<token> expected {
      { "q", 0, 1 },
      { "u", 1, 2 },
      { "i", 2, 3 },
      { "c", 3, 4 },
      { "k", 4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }
  {
    SCOPED_TRACE("1-gram start marker end marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(
        1, 1, false, irs::analysis::ngram_token_stream_base::InputType::Binary,
        irs::ref_cast<irs::byte_type>(irs::string_ref("$")),
        irs::ref_cast<irs::byte_type>(irs::string_ref("^"))));

    const std::vector<token> expected{
      { "$q", 0, 1, "$", irs::string_ref::EMPTY},
      { "u", 1, 2 },
      { "i", 2, 3 },
      { "c", 3, 4 },
      { "k^", 4, 5, irs::string_ref::EMPTY, "^" }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("1-gram, preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 1, true));

    const std::vector<token> expected {
      { "q", 0, 1 },
      {"quick", 0, 5},
      { "u", 1, 2 },
      { "i", 2, 3 },
      { "c", 3, 4 },
      { "k", 4, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("1-gram  preserve original start marker end marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(
        1, 1, true, irs::analysis::ngram_token_stream_base::InputType::Binary,
        irs::ref_cast<irs::byte_type>(irs::string_ref("$")),
        irs::ref_cast<irs::byte_type>(irs::string_ref("^"))));

    const std::vector<token> expected{
      { "$q", 0, 1, "$", irs::string_ref::EMPTY},
      {"$quick", 0, 5, "$", irs::string_ref::EMPTY},
      {"quick^", 0, 5, irs::string_ref::EMPTY, "^"},
      { "u", 1, 2 },
      { "i", 2, 3 },
      { "c", 3, 4 },
      { "k^", 4, 5, irs::string_ref::EMPTY, "^" }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("2-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(2, 2, false));

    const std::vector<token> expected {
      { "qu", 0, 2 },
      { "ui", 1, 3 },
      { "ic", 2, 4 },
      { "ck", 3, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("2-gram, preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(2, 2, true));

    const std::vector<token> expected {
      { "qu", 0, 2 },
      {"quick", 0, 5},
      { "ui", 1, 3 },
      { "ic", 2, 4 },
      { "ck", 3, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("1..2-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 2, false));

    const std::vector<token> expected {
      { "q",  0, 1 },
      { "qu", 0, 2 },
      { "u",  1, 2 },
      { "ui", 1, 3 },
      { "i",  2, 3 },
      { "ic", 2, 4 },
      { "c",  3, 4 },
      { "ck", 3, 5 },
      { "k",  4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }


  {
    SCOPED_TRACE("3-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(3, 3, false));

    const std::vector<token> expected {
      { "qui", 0, 3 },
      { "uic", 1, 4 },
      { "ick", 2, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }


  {
    SCOPED_TRACE("1..3-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 3, false));

    const std::vector<token> expected{
      { "q",   0, 1 },
      { "qu",  0, 2 },
      { "qui", 0, 3 },
      { "u",   1, 2 },
      { "ui",  1, 3 },
      { "uic", 1, 4 },
      { "i",   2, 3 },
      { "ic",  2, 4 },
      { "ick", 2, 5 },
      { "c",   3, 4 },
      { "ck",  3, 5 },
      { "k",   4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }
  {
    SCOPED_TRACE("1..3-gram start marker end marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(
        1, 3, false,
        irs::analysis::ngram_token_stream_base::InputType::Binary,
        irs::ref_cast<irs::byte_type>(irs::string_ref("$")),
        irs::ref_cast<irs::byte_type>(irs::string_ref("^"))));

    const std::vector<token> expected{
      { "$q",   0, 1, "$", irs::string_ref::EMPTY },
      { "$qu",  0, 2, "$", irs::string_ref::EMPTY },
      { "$qui", 0, 3, "$", irs::string_ref::EMPTY },
      { "u",   1, 2 },
      { "ui",  1, 3 },
      { "uic", 1, 4 },
      { "i",   2, 3 },
      { "ic",  2, 4 },
      { "ick^", 2, 5, irs::string_ref::EMPTY, "^" },
      { "c",   3, 4 },
      { "ck^",  3, 5, irs::string_ref::EMPTY, "^" },
      { "k^",   4, 5, irs::string_ref::EMPTY, "^" }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("2..3-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(2, 3, false));

    const std::vector<token> expected {
      { "qu",  0, 2 },
      { "qui", 0, 3 },
      { "ui",  1, 3 },
      { "uic", 1, 4 },
      { "ic",  2, 4 },
      { "ick", 2, 5 },
      { "ck",  3, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }


  {
    SCOPED_TRACE("2..3-gram, preserve origianl");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(2, 3, true));

    const std::vector<token> expected {
      { "qu",  0, 2 },
      { "qui", 0, 3 },
      {"quick", 0, 5},
      { "ui",  1, 3 },
      { "uic", 1, 4 },
      { "ic",  2, 4 },
      { "ick", 2, 5 },
      { "ck",  3, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("2..3-gram, preserve origianl start marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(
        2, 3, true, irs::analysis::ngram_token_stream_base::InputType::Binary,
        irs::ref_cast<irs::byte_type>(irs::string_ref("$")),
        irs::bytes_ref::EMPTY));

    const std::vector<token> expected{
      { "$qu",  0, 2, "$", irs::string_ref::EMPTY  },
      { "$qui", 0, 3, "$", irs::string_ref::EMPTY  },
      { "$quick", 0, 5, "$", irs::string_ref::EMPTY },
      { "ui",  1, 3 },
      { "uic", 1, 4 },
      { "ic",  2, 4 },
      { "ick", 2, 5 },
      { "ck",  3, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("2..3-gram, preserve origianl end marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(2, 3, true,
      irs::analysis::ngram_token_stream_base::InputType::Binary,
      irs::bytes_ref::EMPTY,
      irs::ref_cast<irs::byte_type>(irs::string_ref("^"))));

    const std::vector<token> expected{
      { "qu",  0, 2 },
      { "qui", 0, 3 },
      {"quick^", 0, 5, irs::string_ref::EMPTY, "^"},
      { "ui",  1, 3 },
      { "uic", 1, 4 },
      { "ic",  2, 4 },
      { "ick^", 2, 5, irs::string_ref::EMPTY, "^" },
      { "ck^",  3, 5, irs::string_ref::EMPTY, "^" }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("2..3-gram, preserve origianl start marker end marker");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(2, 3, true,
      irs::analysis::ngram_token_stream_base::InputType::Binary,
      irs::ref_cast<irs::byte_type>(irs::string_ref("$")),
      irs::ref_cast<irs::byte_type>(irs::string_ref("^"))));

    const std::vector<token> expected{
      { "$qu",  0, 2, "$", irs::string_ref::EMPTY  },
      { "$qui", 0, 3, "$", irs::string_ref::EMPTY  },
      { "$quick", 0, 5, "$", irs::string_ref::EMPTY },
      { "quick^", 0, 5, irs::string_ref::EMPTY, "^"},
      { "ui",  1, 3 },
      { "uic", 1, 4 },
      { "ic",  2, 4 },
      { "ick^", 2, 5, irs::string_ref::EMPTY, "^" },
      { "ck^",  3, 5, irs::string_ref::EMPTY, "^" }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("4-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(4, 4, false));

    const std::vector<token> expected {
      { "quic", 0, 4 },
      { "uick", 1, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("1..4-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 4, false));

    const std::vector<token> expected {
      { "q",    0, 1 },
      { "qu",   0, 2 },
      { "qui",  0, 3 },
      { "quic", 0, 4 },
      { "u",    1, 2 },
      { "ui",   1, 3 },
      { "uic",  1, 4 },
      { "uick", 1, 5 },
      { "i",    2, 3 },
      { "ic",   2, 4 },
      { "ick",  2, 5 },
      { "c",    3, 4 },
      { "ck",   3, 5 },
      { "k",    4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("5-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(5, 5, false));

    const std::vector<token> expected {
      { "quick", 0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("5-gram, preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(5, 5, true));

    const std::vector<token> expected {
      { "quick", 0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("4-5-gram, preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(4, 5, true));

    const std::vector<token> expected{
      { "quic", 0, 4 },
      { "quick", 0, 5 },
      { "uick", 1, 5 }
    };
    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("4-5-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(4, 5, false));

    const std::vector<token> expected{
      { "quic", 0, 4 },
      { "quick", 0, 5 },
      { "uick", 1, 5 }
    };
    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("6-gram, preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(6, 6, true));

    const std::vector<token> expected{
      { "quick", 0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("6-gram no output");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(6, 6, false));

    ASSERT_TRUE(stream.reset("quick"));
    ASSERT_FALSE(stream.next());
  }

  {
    SCOPED_TRACE("1..5-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 5, false));

    const std::vector<token> expected {
      { "q",     0, 1 },
      { "qu",    0, 2 },
      { "qui",   0, 3 },
      { "quic",  0, 4 },
      { "quick", 0, 5 },
      { "u",     1, 2 },
      { "ui",    1, 3 },
      { "uic",   1, 4 },
      { "uick",  1, 5 },
      { "i",     2, 3 },
      { "ic",    2, 4 },
      { "ick",   2, 5 },
      { "c",     3, 4 },
      { "ck",    3, 5 },
      { "k",     4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("3..5-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(3, 5, false));

    const std::vector<token> expected {
      { "qui",   0, 3 },
      { "quic",  0, 4 },
      { "quick", 0, 5 },
      { "uic",   1, 4 },
      { "uick",  1, 5 },
      { "ick",   2, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("6-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(6, 6, false));

    const std::vector<token> expected { };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("1..6-gram");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 6, false));

    const std::vector<token> expected {
      { "q",     0, 1 },
      { "qu",    0, 2 },
      { "qui",   0, 3 },
      { "quic",  0, 4 },
      { "quick", 0, 5 },
      { "u",     1, 2 },
      { "ui",    1, 3 },
      { "uic",   1, 4 },
      { "uick",  1, 5 },
      { "i",     2, 3 },
      { "ic",    2, 4 },
      { "ick",   2, 5 },
      { "c",     3, 4 },
      { "ck",    3, 5 },
      { "k",     4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  {
    SCOPED_TRACE("1..6-gram, preserve original");
    irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(irs::analysis::ngram_token_stream_base::Options(1, 6, true));

    const std::vector<token> expected {
      { "q",     0, 1 },
      { "qu",    0, 2 },
      { "qui",   0, 3 },
      { "quic",  0, 4 },
      { "quick", 0, 5 },
      { "u",     1, 2 },
      { "ui",    1, 3 },
      { "uic",   1, 4 },
      { "uick",  1, 5 },
      { "i",     2, 3 },
      { "ic",    2, 4 },
      { "ick",   2, 5 },
      { "c",     3, 4 },
      { "ck",    3, 5 },
      { "k",     4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }
}

TEST(ngram_token_stream_test, test_make_config_json) {
  //with unknown parameter
  {
    std::string config = "{\"min\":1,\"max\":5,\"preserveOriginal\":false,\"invalid_parameter\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "ngram", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"min\":1,\"max\":5,\"preserveOriginal\":false,\"streamType\":\"binary\"}", actual);
  }

  //with changed values
  {
    std::string config = "{\"min\":11,\"max\":22,\"preserveOriginal\":true,\"startMarker\":\"$\",\"endMarker\":\"#\",\"streamType\":\"utf8\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "ngram", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"min\":11,\"max\":22,\"preserveOriginal\":true,\"streamType\":\"utf8\",\"startMarker\":\"$\",\"endMarker\":\"#\"}", actual);
  }

  //with only start marker
  {
    std::string config = "{\"min\":11,\"max\":22,\"preserveOriginal\":true,\"startMarker\":\"$123\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "ngram", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"min\":11,\"max\":22,\"preserveOriginal\":true,\"streamType\":\"binary\",\"startMarker\":\"$123\"}", actual);
  }
  //with only end marker
  {
    std::string config = "{\"min\":11,\"max\":22,\"preserveOriginal\":true,\"endMarker\":\"#456\"}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "ngram", irs::type<irs::text_format::json>::get(), config));
    ASSERT_EQ("{\"min\":11,\"max\":22,\"preserveOriginal\":true,\"streamType\":\"binary\",\"endMarker\":\"#456\"}", actual);
  }
}

//TEST_F(ngram_token_stream_test, test_make_config_text) {
// No text builder for analyzer so far
//}

TEST(ngram_token_stream_test, test_make_config_invalid_format) {
  std::string config = "{\"min\":11,\"max\":22,\"preserveOriginal\":true}";
  std::string actual;
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "ngram", irs::type<irs::text_format::text>::get(), config));
  ASSERT_FALSE(irs::analysis::analyzers::normalize(actual, "ngram", irs::type<irs::text_format::csv>::get(), config));
}


TEST(ngram_token_stream_test, test_out_of_range_pos_issue) {
  auto stream = irs::analysis::analyzers::get(
      "ngram", irs::type<irs::text_format::json>::get(),
      "{\"min\":2,\"max\":3,\"preserveOriginal\":true}");
  ASSERT_NE(nullptr, stream);
  auto* inc = irs::get<irs::increment>(*stream);
  for (size_t i = 0; i < 10000; ++i) {
    std::basic_stringstream<char> ss;
    ss << "test_" << i;
    ASSERT_TRUE(stream->reset(ss.str()));
    uint32_t pos = irs::integer_traits<uint32_t>::const_max;
    uint32_t last_pos = 0;
    while (stream->next()) {
      pos += inc->value;
      ASSERT_GE(pos, last_pos);
      ASSERT_LT(pos, irs::pos_limits::eof());
      last_pos = pos;
    }
  }
}

// Performance tests below are convenient way to quickly analyze performance changes
// However  there is no point to run them as part of regular tests and no point to spoil output by marking them disabled
//
//TEST(ngram_token_stream_test, performance_next_utf8) {
//  irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
//    irs::analysis::ngram_token_stream_base::Options(1, 3, true,
//      irs::analysis::ngram_token_stream_base::InputType::UTF8,
//      irs::bytes_ref::EMPTY, irs::bytes_ref::EMPTY));
//
//  std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
//  for (size_t i = 0; i < 100000; ++i) {
//    sDataUCS2 += L"a\u00A2b\u00A3c\u00A4d\u00A5";
//  }
//  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
//  std::string data;
//  ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
//  //std::cerr << "Set debug breakpoint here";
//  for (size_t i = 0; i < 10; ++i) {
//    stream.reset(data);
//    while (stream.next()) {}
//  }
//  ASSERT_FALSE(stream.next());
//}
//
//TEST(ngram_token_stream_test, performance_next) {
//  irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(
//    irs::analysis::ngram_token_stream_base::Options(1, 3, true));
//
//  std::string data = "quickbro";
//  for (size_t i = 0; i < 100000; ++i) {
//    data += "quickbro";
//  }
//  //std::cerr << "Set debug breakpoint here";
//  for (size_t i = 0; i < 10; ++i) {
//    stream.reset(data);
//    while (stream.next()) {}
//  }
//  ASSERT_FALSE(stream.next());
//}
//
//
//TEST(ngram_token_stream_test, performance_next_utf8_marker) {
//  irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8> stream(
//    irs::analysis::ngram_token_stream_base::Options(1, 3, true,
//      irs::analysis::ngram_token_stream_base::InputType::UTF8,
//      irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa2"), 2), irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa1"), 2)));
//
//  std::wstring sDataUCS2 = L"a\u00A2b\u00A3c\u00A4d\u00A5";
//  for (size_t i = 0; i < 100000; ++i) {
//    sDataUCS2 += L"a\u00A2b\u00A3c\u00A4d\u00A5";
//  }
//  auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
//  std::string data;
//  ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
//  //std::cerr << "Set debug breakpoint here";
//  for (size_t i = 0; i < 10; ++i) {
//    stream.reset(data);
//    while (stream.next()) {}
//  }
//  ASSERT_FALSE(stream.next());
//}
//
//TEST(ngram_token_stream_test, performance_next_marker) {
//  irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary> stream(
//    irs::analysis::ngram_token_stream_base::Options(1, 3, true,
//      irs::analysis::ngram_token_stream_base::InputType::Binary,
//      irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa2"), 2), irs::bytes_ref(reinterpret_cast<const irs::byte_type*>("\xc2\xa1"), 2)));
//
//  std::string data = "quickbro";
//  for (size_t i = 0; i < 100000; ++i) {
//    data += "quickbro";;
//  }
//  //std::cerr << "Set debug breakpoint here";
//  for (size_t i = 0; i < 10; ++i) {
//    stream.reset(data);
//    while (stream.next()) {}
//  }
//  ASSERT_FALSE(stream.next());
//}

#endif // IRESEARCH_DLL

TEST(ngram_token_stream_test, test_load) {
  {
    irs::string_ref data("quick");
    auto stream = irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{ \"min\":5,\"max\":5,\"preserveOriginal\":false,\"streamType\":\"binary\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* term = irs::get<irs::term_attribute>(*stream);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(5, offset->end);
    ASSERT_EQ("quick", irs::ref_cast<char>(term->value));
    ASSERT_EQ(1, inc->value);
    ASSERT_FALSE(stream->next());
  }

  {
    std::wstring sDataUCS2 = L"\u00C0\u00C1\u00C2\u00C3\u00C4";
    auto locale = irs::locale_utils::locale(irs::string_ref::NIL, "utf8", true); // utf8 internal and external
    std::string data;
    ASSERT_TRUE(irs::locale_utils::append_external<wchar_t>(data, sDataUCS2, locale));
    auto stream = irs::analysis::analyzers::get("ngram", irs::type<irs::text_format::json>::get(), "{ \"min\":5,\"max\":5,\"preserveOriginal\":false,\"streamType\":\"utf8\"}");

    ASSERT_NE(nullptr, stream);
    ASSERT_TRUE(stream->reset(data));

    auto* offset = irs::get<irs::offset>(*stream);
    auto* term = irs::get<irs::term_attribute>(*stream);
    auto* inc = irs::get<irs::increment>(*stream);
    ASSERT_TRUE(stream->next());
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(10, offset->end);
    ASSERT_EQ("\xc3\x80\xc3\x81\xc3\x82\xc3\x83\xc3\x84", irs::ref_cast<char>(term->value));
    ASSERT_EQ(1, inc->value);
    ASSERT_FALSE(stream->next());
  }
}
