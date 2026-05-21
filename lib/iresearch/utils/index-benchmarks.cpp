////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include <functional>

#include "analysis/collation_token_stream.hpp"
#include "analysis/delimited_token_stream.hpp"
#include "analysis/ngram_token_stream.hpp"
#include "analysis/pipeline_token_stream.hpp"
#include "analysis/segmentation_token_stream.hpp"
#include "analysis/text_token_normalizing_stream.hpp"
#include "analysis/text_token_stemming_stream.hpp"
#include "analysis/text_token_stream.hpp"
#include "analysis/token_stopwords_stream.hpp"
#include "index-put.hpp"
#include "index-search.hpp"

#include <absl/container/flat_hash_map.h>

namespace {

using handlers_t =
  absl::flat_hash_map<std::string, std::function<int(int argc, char* argv[])>>;

void init_analyzers() {
  ::irs::analysis::delimited_token_stream::init();
  ::irs::analysis::collation_token_stream::init();
  ::irs::analysis::ngram_token_stream_base::init();
  ::irs::analysis::normalizing_token_stream::init();
  ::irs::analysis::stemming_token_stream::init();
  ::irs::analysis::text_token_stream::init();
  ::irs::analysis::token_stopwords_stream::init();
  ::irs::analysis::pipeline_token_stream::init();
  ::irs::analysis::segmentation_token_stream::init();
}

}  // namespace

bool init_handlers(handlers_t& handlers) {
  init_analyzers();
  handlers.emplace("put", &put);
  handlers.emplace("search", &search);
  return true;
}
