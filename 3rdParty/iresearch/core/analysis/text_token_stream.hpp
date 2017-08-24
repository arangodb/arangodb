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

#ifndef IRESEARCH_IQL_TEXT_TOKEN_STREAM_H
#define IRESEARCH_IQL_TEXT_TOKEN_STREAM_H

#include "analyzers.hpp"
#include "shared.hpp"
#include "token_stream.hpp"
#include "token_attributes.hpp"

NS_ROOT
NS_BEGIN(analysis)

class text_token_stream : public analyzer, util::noncopyable {
 public:
  struct state_t;

  class bytes_term : public irs::term_attribute {
   public:
    void clear() {
      buf_.clear();
      value_ = irs::bytes_ref::nil;
    }

    void value(irs::bstring&& data) {
      buf_ = std::move(data);
      value(buf_);
    }

    void value(const irs::bytes_ref& data) {
      value_ = data;
    }

   private:
    irs::bstring buf_; // buffer for value if value cannot be referenced directly
  };

  static char const* STOPWORD_PATH_ENV_VARIABLE;

  DECLARE_ANALYZER_TYPE();

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief args is a locale name, TODO use below if switching to JSON config
  /// @brief args is a jSON encoded object with the following attributes:
  ///        "locale"(string): locale of the analyzer <required>
  ///        "ignored_words([string...]): set of words to ignore (missing == use default list)
  ////////////////////////////////////////////////////////////////////////////////
  DECLARE_FACTORY_DEFAULT(const string_ref& args);

  text_token_stream(
    const std::locale& locale,
    const std::unordered_set<std::string>& ignored_words
  );
  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }
  virtual bool next() override;
  virtual bool reset(const string_ref& data) override;

 private:
  irs::attribute_view attrs_;
  std::shared_ptr<state_t> state_;
  struct {
    std::string country;
    std::string encoding;
    std::string language;
    bool utf8;
  } locale_;
  const std::unordered_set<std::string>& ignored_words_;
  irs::offset offs_;
  irs::increment inc_;
  bytes_term term_;
};

NS_END // analysis
NS_END // ROOT

#endif
