////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "IResearch/IResearchFilterOptimization.h"
#include "search/levenshtein_filter.hpp"
#include "search/prefix_filter.hpp"

namespace arangodb {
namespace iresearch {

bool includeStartsWithInLevenshtein(irs::boolean_filter* filter,
                                    std::string_view name,
                                    std::string_view startsWith) {
  if (filter->type() == irs::type<irs::And>::id()) {
    for (auto& f : *filter) {
      if (f->type() == irs::type<irs::by_edit_distance>::id()) {
        auto& levenshtein = static_cast<irs::by_edit_distance&>(*f);
        if (levenshtein.field() == name) {
          auto options = levenshtein.mutable_options();
          if (startsWith.size() <= options->prefix.size()) {
            if (irs::ViewCast<char>(irs::bytes_view{options->prefix})
                    .starts_with(startsWith)) {
              // Nothing to do. We are already covered by this levenshtein
              // prefix
              return true;
            }
          } else {
            // maybe we could enlarge prefix to cover us?
            if (startsWith.starts_with(
                    irs::ViewCast<char>(irs::bytes_view{options->prefix}))) {
              // looks promising - beginning of the levenshtein prefix is ok
              auto prefixTailSize = startsWith.size() - options->prefix.size();
              if (irs::ViewCast<char>(irs::bytes_view{options->term})
                      .starts_with(std::string_view{
                          startsWith.data() + options->prefix.size(),
                          prefixTailSize})) {
                // we could enlarge prefix
                options->prefix = irs::ViewCast<irs::byte_type>(startsWith);
                options->term.erase(options->term.begin(),
                                    options->term.begin() + prefixTailSize);
                return true;
              }
            }
          }
          if ((options->term.size() + options->prefix.size() +
               options->max_distance) < startsWith.size()) {
            // last optimization effort - we can't fulfill this conjunction.
            // make it empty
            filter->clear();
            filter->add<irs::empty>();
            return true;
          }
        }
      }
    }
  }
  return false;
}

}  // namespace iresearch
}  // namespace arangodb
