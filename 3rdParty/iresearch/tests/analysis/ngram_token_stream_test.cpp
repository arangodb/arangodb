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

#include "tests_shared.hpp"
#include "analysis/ngram_token_stream.hpp"

#ifndef IRESEARCH_DLL

TEST(ngram_token_stream_test, construct) {
  // load jSON object
  {
    auto stream = irs::analysis::analyzers::get("ngram", irs::text_format::json, "{\"min\":1, \"max\":3, \"preserveOriginal\":true}");
    ASSERT_NE(nullptr, stream);

    auto& impl = dynamic_cast<irs::analysis::ngram_token_stream&>(*stream);
    ASSERT_EQ(1, impl.min_gram());
    ASSERT_EQ(3, impl.max_gram());
    ASSERT_EQ(true, impl.preserve_original());
  }

  // load jSON object
  {
    auto stream = irs::analysis::analyzers::get("ngram", irs::text_format::json, "{\"min\":0, \"max\":1, \"preserveOriginal\":false, \"invalidProperty\":true }");
    ASSERT_NE(nullptr, stream);

    auto& impl = dynamic_cast<irs::analysis::ngram_token_stream&>(*stream);
    ASSERT_EQ(1, impl.min_gram());
    ASSERT_EQ(1, impl.max_gram());
    ASSERT_EQ(false, impl.preserve_original());
  }

  // load jSON invalid
  {
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, irs::string_ref::NIL));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, "1"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, "[]"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, "{}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, "{\"locale\":1}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, "{\"min\":\"1\", \"max\":3, \"preserveOriginal\":true}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, "{\"min\":1, \"max\":\"3\", \"preserveOriginal\":true}"));
    ASSERT_EQ(nullptr, irs::analysis::analyzers::get("ngram", irs::text_format::json, "{\"min\":1, \"max\":3, \"preserveOriginal\":\"true\"}"));
  }

  // 2-gram
  {
    auto stream = irs::analysis::ngram_token_stream::make(irs::analysis::ngram_token_stream::options_t(2, 2, true));
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::analysis::ngram_token_stream::type(), stream->type());

    auto impl = std::dynamic_pointer_cast<irs::analysis::ngram_token_stream>(stream);
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ(2, impl->min_gram());
    ASSERT_EQ(2, impl->max_gram());
    ASSERT_EQ(true, impl->preserve_original());

    ASSERT_EQ(3, stream->attributes().size());

    auto& term = stream->attributes().get<irs::term_attribute>();
    ASSERT_TRUE(term);
    ASSERT_TRUE(term->value().null());

    auto& increment = stream->attributes().get<irs::increment>();
    ASSERT_TRUE(increment);
    ASSERT_EQ(1, increment->value);

    auto& offset = stream->attributes().get<irs::offset>();
    ASSERT_TRUE(offset);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(0, offset->end);
  }

  // 0 == min_gram
  {
    auto stream = irs::analysis::ngram_token_stream::make(irs::analysis::ngram_token_stream::options_t(0, 2, true));
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::analysis::ngram_token_stream::type(), stream->type());

    auto impl = std::dynamic_pointer_cast<irs::analysis::ngram_token_stream>(stream);
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ(1, impl->min_gram());
    ASSERT_EQ(2, impl->max_gram());
    ASSERT_EQ(true, impl->preserve_original());

    ASSERT_EQ(3, stream->attributes().size());

    auto& term = stream->attributes().get<irs::term_attribute>();
    ASSERT_TRUE(term);
    ASSERT_TRUE(term->value().null());

    auto& increment = stream->attributes().get<irs::increment>();
    ASSERT_TRUE(increment);
    ASSERT_EQ(1, increment->value);

    auto& offset = stream->attributes().get<irs::offset>();
    ASSERT_TRUE(offset);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(0, offset->end);
  }

  // min_gram > max_gram
  {
    auto stream = irs::analysis::ngram_token_stream::make(irs::analysis::ngram_token_stream::options_t(std::numeric_limits<size_t>::max(), 2, true));
    ASSERT_NE(nullptr, stream);
    ASSERT_EQ(irs::analysis::ngram_token_stream::type(), stream->type());

    auto impl = std::dynamic_pointer_cast<irs::analysis::ngram_token_stream>(stream);
    ASSERT_NE(nullptr, impl);
    ASSERT_EQ(std::numeric_limits<size_t>::max(), impl->min_gram());
    ASSERT_EQ(std::numeric_limits<size_t>::max(), impl->max_gram());
    ASSERT_EQ(true, impl->preserve_original());

    ASSERT_EQ(3, stream->attributes().size());

    auto& term = stream->attributes().get<irs::term_attribute>();
    ASSERT_TRUE(term);
    ASSERT_TRUE(term->value().null());

    auto& increment = stream->attributes().get<irs::increment>();
    ASSERT_TRUE(increment);
    ASSERT_EQ(1, increment->value);

    auto& offset = stream->attributes().get<irs::offset>();
    ASSERT_TRUE(offset);
    ASSERT_EQ(0, offset->start);
    ASSERT_EQ(0, offset->end);
  }
}

//FIXME
//TEST(ngram_token_stream_test, next_utf8) { }


TEST(ngram_token_stream_test, reset_too_big) {
  irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 1, false));

  const irs::string_ref input(
    reinterpret_cast<const char*>(&stream),
    size_t(std::numeric_limits<uint32_t>::max()) + 1
  );

  ASSERT_FALSE(stream.reset(input));
  ASSERT_EQ(3, stream.attributes().size());

  auto& term = stream.attributes().get<irs::term_attribute>();
  ASSERT_TRUE(term);
  ASSERT_TRUE(term->value().null());

  auto& increment = stream.attributes().get<irs::increment>();
  ASSERT_TRUE(increment);
  ASSERT_EQ(1, increment->value);

  auto& offset = stream.attributes().get<irs::offset>();
  ASSERT_TRUE(offset);
  ASSERT_EQ(0, offset->start);
  ASSERT_EQ(0, offset->end);
}

TEST(ngram_token_stream_test, next) {
  struct token {
    token(const irs::string_ref& value, size_t start, size_t end) NOEXCEPT
      : value(value), start(start), end(end) {
    }

    irs::string_ref value;
    size_t start;
    size_t end;
  };

  auto assert_tokens = [](
      const std::vector<token>& expected,
      const irs::string_ref& data,
      irs::analysis::ngram_token_stream& stream) {
    ASSERT_TRUE(stream.reset(data));

    auto& value = stream.attributes().get<irs::term_attribute>();
    ASSERT_TRUE(value);

    auto& offset = stream.attributes().get<irs::offset>();
    ASSERT_TRUE(offset);

    auto expected_token = expected.begin();
    while (stream.next()) {
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(expected_token->value), value->value());
      ASSERT_EQ(expected_token->start, offset->start);
      ASSERT_EQ(expected_token->end, offset->end);
      ++expected_token;
    }
    ASSERT_EQ(expected_token, expected.end());
    ASSERT_FALSE(stream.next());
  };

  // 1-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 1, false));

    const std::vector<token> expected {
      { "q", 0, 1 },
      { "u", 1, 2 },
      { "i", 2, 3 },
      { "c", 3, 4 },
      { "k", 4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 1-gram, preserve original
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 1, true));

    const std::vector<token> expected {
      { "q", 0, 1 },
      { "u", 1, 2 },
      { "i", 2, 3 },
      { "c", 3, 4 },
      { "k", 4, 5 },
      { "quick", 0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 2-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(2, 2, false));

    const std::vector<token> expected {
      { "qu", 0, 2 },
      { "ui", 1, 3 },
      { "ic", 2, 4 },
      { "ck", 3, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  // 2-gram, preserver original
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(2, 2, true));

    const std::vector<token> expected {
      { "qu", 0, 2 },
      { "ui", 1, 3 },
      { "ic", 2, 4 },
      { "ck", 3, 5 },
      { "quick", 0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 1..2-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 2, false));

    const std::vector<token> expected {
      { "qu", 0, 2 },
      { "q",  0, 1 },
      { "ui", 1, 3 },
      { "u",  1, 2 },
      { "ic", 2, 4 },
      { "i",  2, 3 },
      { "ck", 3, 5 },
      { "c",  3, 4 },
      { "k",  4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 3-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(3, 3, false));

    const std::vector<token> expected {
      { "qui", 0, 3 },
      { "uic", 1, 4 },
      { "ick", 2, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 1..3-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 3, false));

    const std::vector<token> expected {
      { "qui", 0, 3 },
      { "qu",  0, 2 },
      { "q",   0, 1 },
      { "uic", 1, 4 },
      { "ui",  1, 3 },
      { "u",   1, 2 },
      { "ick", 2, 5 },
      { "ic",  2, 4 },
      { "i",   2, 3 },
      { "ck",  3, 5 },
      { "c",   3, 4 },
      { "k",   4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 2..3-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(2, 3, false));

    const std::vector<token> expected {
      { "qui", 0, 3 },
      { "qu",  0, 2 },
      { "uic", 1, 4 },
      { "ui",  1, 3 },
      { "ick", 2, 5 },
      { "ic",  2, 4 },
      { "ck",  3, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  // 2..3-gram, preserve origianl
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(2, 3, true));

    const std::vector<token> expected {
      { "qui", 0, 3 },
      { "qu",  0, 2 },
      { "uic", 1, 4 },
      { "ui",  1, 3 },
      { "ick", 2, 5 },
      { "ic",  2, 4 },
      { "ck",  3, 5 },
      { "quick",  0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 4-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t( 4, 4, false ));

    const std::vector<token> expected {
      { "quic", 0, 4 },
      { "uick", 1, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  // 1..4-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 4, false));

    const std::vector<token> expected {
      { "quic", 0, 4 },
      { "qui",  0, 3 },
      { "qu",   0, 2 },
      { "q",    0, 1 },
      { "uick", 1, 5 },
      { "uic",  1, 4 },
      { "ui",   1, 3 },
      { "u",    1, 2 },
      { "ick",  2, 5 },
      { "ic",   2, 4 },
      { "i",    2, 3 },
      { "ck",   3, 5 },
      { "c",    3, 4 },
      { "k",    4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 5-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(5, 5, false));

    const std::vector<token> expected {
      { "quick", 0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 5-gram, preserve original
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(5, 5, true));

    const std::vector<token> expected {
      { "quick", 0, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 1..5-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 5, false));

    const std::vector<token> expected {
      { "quick", 0, 5 },
      { "quic",  0, 4 },
      { "qui",   0, 3 },
      { "qu",    0, 2 },
      { "q",     0, 1 },
      { "uick",  1, 5 },
      { "uic",   1, 4 },
      { "ui",    1, 3 },
      { "u",     1, 2 },
      { "ick",   2, 5 },
      { "ic",    2, 4 },
      { "i",     2, 3 },
      { "ck",    3, 5 },
      { "c",     3, 4 },
      { "k",     4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 3..5-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(3, 5, false));

    const std::vector<token> expected {
      { "quick", 0, 5 },
      { "quic",  0, 4 },
      { "qui",   0, 3 },
      { "uick",  1, 5 },
      { "uic",   1, 4 },
      { "ick",   2, 5 },
    };

    assert_tokens(expected, "quick", stream);
  }

  // 6-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(6, 6, false));

    const std::vector<token> expected { };

    assert_tokens(expected, "quick", stream);
  }

  // 1..6-gram
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 6, false));

    const std::vector<token> expected {
      { "quick", 0, 5 },
      { "quic",  0, 4 },
      { "qui",   0, 3 },
      { "qu",    0, 2 },
      { "q",     0, 1 },
      { "uick",  1, 5 },
      { "uic",   1, 4 },
      { "ui",    1, 3 },
      { "u",     1, 2 },
      { "ick",   2, 5 },
      { "ic",    2, 4 },
      { "i",     2, 3 },
      { "ck",    3, 5 },
      { "c",     3, 4 },
      { "k",     4, 5 }
    };

    assert_tokens(expected, "quick", stream);
  }

  // 1..6-gram, preserve original
  {
    irs::analysis::ngram_token_stream stream(irs::analysis::ngram_token_stream::options_t(1, 6, true));

    const std::vector<token> expected {
      { "quick", 0, 5 },
      { "quic",  0, 4 },
      { "qui",   0, 3 },
      { "qu",    0, 2 },
      { "q",     0, 1 },
      { "uick",  1, 5 },
      { "uic",   1, 4 },
      { "ui",    1, 3 },
      { "u",     1, 2 },
      { "ick",   2, 5 },
      { "ic",    2, 4 },
      { "i",     2, 3 },
      { "ck",    3, 5 },
      { "c",     3, 4 },
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
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "ngram", irs::text_format::json, config));
    ASSERT_EQ("{\"min\":1,\"max\":5,\"preserveOriginal\":false}", actual);
  }

  //with changed values
  {
    std::string config = "{\"min\":11,\"max\":22,\"preserveOriginal\":true}";
    std::string actual;
    ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "ngram", irs::text_format::json, config));
    ASSERT_EQ("{\"min\":11,\"max\":22,\"preserveOriginal\":true}", actual);
  }
}

//TEST_F(ngram_token_stream_test, test_make_config_text) {
// No text builder for analyzer so far
//}

TEST(ngram_token_stream_test, test_make_config_invalid_format) {
  std::string config = "{\"min\":11,\"max\":22,\"preserveOriginal\":true}";
  std::string actual;
  ASSERT_TRUE(irs::analysis::analyzers::normalize(actual, "ngram", irs::text_format::json, config));
}

#endif // IRESEARCH_DLL
