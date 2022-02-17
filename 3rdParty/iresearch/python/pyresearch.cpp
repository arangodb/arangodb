////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "pyresearch.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "store/mmap_directory.hpp"

NS_LOCAL

irs::flags to_flags(const std::vector<std::string>& names) {
  irs::flags features;

  for (const auto& name : names) {
    const auto* feature = irs::attribute::type_id::get(name);

    if (!feature) {
      continue;
    }

    features.add(*feature);
  }

  return features;
}

NS_END

std::vector<std::string> field_reader::features() const {
  std::vector<std::string> result;
  for (const auto* type_id : field_->meta().features) {
    auto& type_name = type_id->name();
    result.emplace_back(type_name.c_str(), type_name.size())    ;
  }
  return result;
}

/*static*/ index_reader index_reader::open(const char* path) {
  auto dir = std::make_shared<irs::mmap_directory>(path);
  auto index = irs::directory_reader::open(*dir);

  return std::shared_ptr<const irs::index_reader>(
    static_cast<irs::index_reader::ptr>(index).get(),
    [dir, index](const irs::index_reader*) { }
  );
}

index_reader::index_reader(std::shared_ptr<const irs::index_reader> reader)
  : reader_(std::move(reader)) {
  assert(reader_);
  segments_.reserve(reader_->size()); 

  for (auto& segment : *reader_) {
    segments_.push_back(&segment);
  }
}

segment_reader index_reader::segment(size_t i) const {
  auto* segment = segments_.at(i);

  return std::shared_ptr<const irs::sub_reader>(
    std::shared_ptr<const irs::sub_reader>(),
    segment
  );
}

doc_iterator term_iterator::postings(
    const std::vector<std::string>& features /*= std::vector<std::string>()*/
) const {
  return it_->postings(to_flags(features));
}
