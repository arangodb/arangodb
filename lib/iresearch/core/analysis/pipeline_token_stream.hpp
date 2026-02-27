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

#pragma once

#include "analyzers.hpp"
#include "shared.hpp"
#include "token_attributes.hpp"
#include "token_stream.hpp"
#include "utils/attribute_helper.hpp"

namespace irs {
namespace analysis {

// An analyser capable of chaining other analyzers
class pipeline_token_stream final : public TypedAnalyzer<pipeline_token_stream>,
                                    private util::noncopyable {
 public:
  using options_t = std::vector<irs::analysis::analyzer::ptr>;

  static constexpr std::string_view type_name() noexcept { return "pipeline"; }
  static void init();  // for triggering registration in a static build

  explicit pipeline_token_stream(options_t&& options);
  attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    auto attr = irs::get_mutable(attrs_, id);
    if (!attr) {
      // if attribute is not strictly pipeline-controlled let`s find nearest to
      // end provider with desired attribute
      if (irs::type<payload>::id() != id && irs::type<offset>::id() != id) {
        for (auto it = rbegin(pipeline_); it != rend(pipeline_); ++it) {
          attr = const_cast<analyzer&>(it->get_stream()).get_mutable(id);
          if (attr) {
            break;
          }
        }
      }
    }
    return attr;
  }
  bool next() final;
  bool reset(std::string_view data) final;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief calls visitor on pipeline members in respective order. Visiting is
  /// interrupted on first visitor returning false.
  /// @param visitor visitor to call
  /// @return true if all visits returned true, false otherwise
  //////////////////////////////////////////////////////////////////////////////
  template<typename Visitor>
  bool visit_members(Visitor&& visitor) const {
    for (const auto& sub : pipeline_) {
      const auto& stream = sub.get_stream();
      if (stream.type() == type()) {
        // pipe inside pipe - forward visiting
        const auto& sub_pipe = DownCast<pipeline_token_stream>(stream);
        if (!sub_pipe.visit_members(visitor)) {
          return false;
        }
      } else if (!visitor(sub.get_stream())) {
        return false;
      }
    }
    return true;
  }

 private:
  struct sub_analyzer_t {
    explicit sub_analyzer_t(irs::analysis::analyzer::ptr a, bool track_offset);
    sub_analyzer_t();

    bool reset(uint32_t start, uint32_t end, std::string_view data) {
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
      IRS_ASSERT(offs);
      return data_start + offs->start;
    }

    uint32_t end() const noexcept {
      IRS_ASSERT(offs);
      return offs->end == data_size ? data_end
                                    : start() + offs->end - offs->start;
    }
    const term_attribute* term;
    const increment* inc;
    const offset* offs;
    size_t data_size{0};
    uint32_t data_start{0};
    uint32_t data_end{0};
    uint32_t pos{std::numeric_limits<uint32_t>::max()};

    const irs::analysis::analyzer& get_stream() const noexcept {
      IRS_ASSERT(analyzer);
      return *analyzer;
    }

   private:
    // sub analyzer should be operated through provided next/release
    // methods to properly track positions/offsets
    irs::analysis::analyzer::ptr analyzer;
  };
  using pipeline_t = std::vector<sub_analyzer_t>;
  using attributes = std::tuple<increment, attribute_ptr<term_attribute>,
                                attribute_ptr<offset>, attribute_ptr<payload>>;

  pipeline_t pipeline_;
  pipeline_t::iterator current_;
  pipeline_t::iterator top_;
  pipeline_t::iterator bottom_;
  offset offs_;
  attributes attrs_;
};

}  // namespace analysis
}  // namespace irs
