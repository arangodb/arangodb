////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_PIPELINE_TOKEN_STREAM_H
#define IRESEARCH_PIPELINE_TOKEN_STREAM_H

#include "shared.hpp"
#include "analyzers.hpp"
#include "token_stream.hpp"
#include "token_attributes.hpp"
#include "utils/frozen_attributes.hpp"

namespace iresearch {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/// @class pipeline_token_stream
/// @brief an analyser capable of chaining other analyzers
////////////////////////////////////////////////////////////////////////////////
class pipeline_token_stream final
  : public analyzer,
    private util::noncopyable {
 public:
  using options_t = std::vector<irs::analysis::analyzer::ptr>;

  static constexpr string_ref type_name() noexcept { return "pipeline"; }
  static void init(); // for triggering registration in a static build

  explicit pipeline_token_stream(options_t&& options);
  virtual attribute* get_mutable(irs::type_info::type_id id) noexcept override {
    return irs::get_mutable(attrs_, id);
  }
  virtual bool next() override;
  virtual bool reset(const string_ref& data) override;

 private:
  struct sub_analyzer_t {
    explicit sub_analyzer_t(const irs::analysis::analyzer::ptr& a, bool track_offset);
    sub_analyzer_t();

    bool reset(uint32_t start, uint32_t end, const string_ref& data) {
      data_size = data.size();
      data_start = start;
      data_end = end;
      pos = std::numeric_limits<uint32_t>::max();
      return analyzer->reset(data);
    }
    bool next() {
      if (analyzer->next()) {
        pos += inc->value;
        return true;
      }
      return false;
    }

    uint32_t start() const noexcept {
      assert(offs);
      return data_start + offs->start;
    }

    uint32_t end() const noexcept {
      assert(offs);
      return offs->end == data_size
        ? data_end
        : start() + offs->end - offs->start;
    }
    const term_attribute* term;
    const increment* inc;
    const offset* offs;
    size_t data_size{ 0 };
    uint32_t data_start{ 0 };
    uint32_t data_end{ 0 };
    uint32_t pos{ std::numeric_limits<uint32_t>::max() };

   private:
    // sub analyzer should be operated through provided next/release
    // methods to properly track positions/offsets
    irs::analysis::analyzer::ptr analyzer;
  };
  using pipeline_t = std::vector<sub_analyzer_t>;
  using attributes = std::tuple<
    increment,
    attribute_ptr<term_attribute>,
    attribute_ptr<offset>,
    attribute_ptr<payload>>;

  pipeline_t pipeline_;
  pipeline_t::iterator current_;
  pipeline_t::iterator top_;
  pipeline_t::iterator bottom_;
  offset offs_;
  attributes attrs_;
};

}
}

#endif // IRESEARCH_PIPELINE_TOKEN_STREAM_H
