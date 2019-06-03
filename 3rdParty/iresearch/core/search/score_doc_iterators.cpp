////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "score_doc_iterators.hpp"

NS_ROOT

doc_iterator_base::doc_iterator_base(const order::prepared& ord)
  : ord_(&ord) {
}

basic_doc_iterator_base::basic_doc_iterator_base(const order::prepared& ord)
  : doc_iterator_base(ord) {
}

void basic_doc_iterator_base::prepare_score(order::prepared::scorers&& scorers) {
  struct scorer_ref {
    explicit scorer_ref(const std::pair<sort::scorer::ptr, size_t>& bucket) NOEXCEPT
      : scorer(bucket.first.get()), offset(bucket.second) {
      assert(scorer);
    }

    irs::sort::scorer* scorer;
    size_t offset;
  }; // scorer_ref

  scorers_ = std::move(scorers);

  switch (scorers_.size()) {
    case 0: {
      // let order initialize empty score
      doc_iterator_base::prepare_score([](byte_type*){ });
    } break;
    case 1: {
      const scorer_ref ref(scorers_[0]);

      if (ref.offset) {
        doc_iterator_base::prepare_score([ref](byte_type* score) {
          ref.scorer->score(score + ref.offset);
        });
      } else {
        auto* scorer = ref.scorer;

        doc_iterator_base::prepare_score([scorer](byte_type* score) {
          scorer->score(score);
        });
      }
    } break;
    case 2: {
      const scorer_ref first(scorers_[0]);
      const scorer_ref second(scorers_[1]);

      if (first.offset) {
        doc_iterator_base::prepare_score([first, second](byte_type* score) {
          first.scorer->score(score + first.offset);
          second.scorer->score(score + second.offset);
        });
      } else {
        doc_iterator_base::prepare_score([first, second](byte_type* score) {
          first.scorer->score(score);
          second.scorer->score(score + second.offset);
        });
      }
    } break;
    default: {
      doc_iterator_base::prepare_score([this](byte_type* score) {
        scorers_.score(score);
      });
    } break;
  }
}

#if defined(_MSC_VER)
  #pragma warning( disable : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wparentheses"
#endif

basic_doc_iterator::basic_doc_iterator(
    const sub_reader& segment,
    const term_reader& field,
    const attribute_store& stats,
    doc_iterator::ptr&& it,
    const order::prepared& ord,
    cost::cost_t estimation) NOEXCEPT
  : basic_doc_iterator_base(ord),
    it_(std::move(it)),
    stats_(&stats) {
  assert(it_);

  // set estimation value
  estimate(estimation);

  // make document attribute accessible
  doc_ = (attrs_.emplace<irs::document>()
            = it_->attributes().get<irs::document>()).get();
  assert(doc_);

  // set scorers
  prepare_score(ord_->prepare_scorers(
    segment, field, *stats_, it_->attributes()
  ));
}

#if defined(_MSC_VER)
  #pragma warning( default : 4706 )
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

NS_END // ROOT
