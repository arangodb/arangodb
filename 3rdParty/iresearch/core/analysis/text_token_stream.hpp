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

NS_ROOT

struct offset;
struct term_attribute;

NS_BEGIN(analysis)

class text_token_stream: public analyzer {
 public:
  struct state_t;
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
  virtual const irs::attribute_store& attributes() const NOEXCEPT override;
  virtual bool next() override;
  virtual bool reset(const string_ref& data) override;

 private:
  irs::attribute_store attrs_;
  std::shared_ptr<state_t> state_;
  struct {
    std::string country;
    std::string encoding;
    std::string language;
    bool utf8;
  } locale_;
  const std::unordered_set<std::string>& ignored_words_;
  iresearch::offset* offs_;
  iresearch::term_attribute* term_;
};

NS_END // analysis
NS_END // ROOT

#endif
