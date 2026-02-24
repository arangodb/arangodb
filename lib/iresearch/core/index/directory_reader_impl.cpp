
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "directory_reader_impl.hpp"

#include "shared.hpp"
#include "utils/directory_utils.hpp"

#include <absl/container/flat_hash_map.h>
#include <absl/strings/str_cat.h>

namespace irs {

struct DirectoryReaderImpl::Init {
  Init(const directory& dir, const DirectoryMeta& meta);

  FileRefs file_refs;
  uint64_t docs_count{};
  uint64_t live_docs_count{};
};

DirectoryReaderImpl::Init::Init(const directory& dir,
                                const DirectoryMeta& meta) {
  const bool has_segments_file = !meta.filename.empty();

  file_refs.reserve(meta.index_meta.segments.size() +
                    size_t{has_segments_file});

  auto& refs = dir.attributes().refs();
  for (const auto& [filename, segment] : meta.index_meta.segments) {
    file_refs.emplace_back(refs.add(filename));
    docs_count += segment.docs_count;
    live_docs_count += segment.live_docs_count;
  }

  if (has_segments_file) {
    file_refs.emplace_back(refs.add(meta.filename));
  }
}

namespace {

MSVC_ONLY(__pragma(warning(push)))
MSVC_ONLY(__pragma(warning(disable : 4457)))  // variable hides function param
index_file_refs::ref_t LoadNewestIndexMeta(IndexMeta& meta,
                                           const directory& dir,
                                           format::ptr& codec) noexcept {
  // if a specific codec was specified
  if (codec) {
    try {
      auto reader = codec->get_index_meta_reader();

      if (!reader) {
        return nullptr;
      }

      index_file_refs::ref_t ref;
      std::string filename;

      // ensure have a valid ref to a filename
      while (!ref) {
        const bool index_exists = reader->last_segments_file(dir, filename);

        if (!index_exists) {
          return nullptr;
        }

        ref = directory_utils::Reference(const_cast<directory&>(dir), filename);
      }

      IRS_ASSERT(ref);
      reader->read(dir, meta, *ref);

      return ref;
    } catch (const std::exception& e) {
      IRS_LOG_ERROR(
        absl::StrCat("Caught exception while reading index meta with codec ''",
                     codec->type()().name(), "', error '", e.what(), "'"));
      return nullptr;
    } catch (...) {
      IRS_LOG_ERROR(
        absl::StrCat("Caught exception while reading index meta with codec ''",
                     codec->type()().name(), "'"));

      return nullptr;
    }
  }

  std::vector<std::string_view> codecs;
  auto visitor = [&codecs](std::string_view name) -> bool {
    codecs.emplace_back(name);
    return true;
  };

  if (!formats::visit(visitor)) {
    return nullptr;
  }

  struct {
    std::time_t mtime;
    index_meta_reader::ptr reader;
    index_file_refs::ref_t ref;
  } newest;

  newest.mtime = (std::numeric_limits<time_t>::min)();

  try {
    for (const std::string_view name : codecs) {
      codec = formats::get(name);

      if (!codec) {
        continue;  // try the next codec
      }

      auto reader = codec->get_index_meta_reader();

      if (!reader) {
        continue;  // try the next codec
      }

      index_file_refs::ref_t ref;
      std::string filename;

      // ensure have a valid ref to a filename
      while (!ref) {
        const bool index_exists = reader->last_segments_file(dir, filename);

        if (!index_exists) {
          break;  // try the next codec
        }

        ref = directory_utils::Reference(const_cast<directory&>(dir), filename);
      }

      // initialize to a value that will never pass 'if' below (to make valgrind
      // happy)
      std::time_t mtime = std::numeric_limits<std::time_t>::min();

      if (ref && dir.mtime(mtime, *ref) && mtime > newest.mtime) {
        newest.mtime = std::move(mtime);
        newest.reader = std::move(reader);
        newest.ref = std::move(ref);
      }
    }

    if (!newest.reader || !newest.ref) {
      return nullptr;
    }

    newest.reader->read(dir, meta, *(newest.ref));

    return newest.ref;
  } catch (const std::exception& e) {
    IRS_LOG_ERROR(absl::StrCat(
      "Caught exception while loading the newest index meta, error '", e.what(),
      "'"));
  } catch (...) {
    IRS_LOG_ERROR("Caught exception while loading the newest index meta");
  }

  return nullptr;
}
MSVC_ONLY(__pragma(warning(pop)))

}  // namespace

DirectoryReaderImpl::DirectoryReaderImpl(const directory& dir,
                                         format::ptr codec,
                                         const IndexReaderOptions& opts,
                                         DirectoryMeta&& meta,
                                         ReadersType&& readers)
  : DirectoryReaderImpl{Init{dir, meta},  dir,
                        std::move(codec), opts,
                        std::move(meta),  std::move(readers)} {}

DirectoryReaderImpl::DirectoryReaderImpl(Init&& init, const directory& dir,
                                         format::ptr&& codec,
                                         const IndexReaderOptions& opts,
                                         DirectoryMeta&& meta,
                                         ReadersType&& readers)
  : CompositeReaderImpl{std::move(readers), init.live_docs_count,
                        init.docs_count},
    dir_{dir},
    codec_{std::move(codec)},
    file_refs_{std::move(init.file_refs)},
    meta_{std::move(meta)},
    opts_{opts} {}

std::shared_ptr<const DirectoryReaderImpl> DirectoryReaderImpl::Open(
  const directory& dir, const IndexReaderOptions& opts, format::ptr codec,
  const std::shared_ptr<const DirectoryReaderImpl>& cached) {
  IndexMeta index_meta;
  auto meta_file_ref = LoadNewestIndexMeta(index_meta, dir, codec);

  if (!meta_file_ref) {
    throw index_not_found{};
  }

  IRS_ASSERT(codec);

  if (cached && cached->meta_.index_meta == index_meta) {
    return cached;  // no changes to refresh
  }

  static constexpr size_t kInvalidCandidate{std::numeric_limits<size_t>::max()};
  absl::flat_hash_map<std::string_view, size_t> reuse_candidates;

  if (cached) {
    const auto& segments = cached->Meta().index_meta.segments;
    reuse_candidates.reserve(segments.size());

    for (size_t i = 0; const auto& segment : segments) {
      IRS_ASSERT(cached);  // ensured by loop condition above
      auto it = reuse_candidates.emplace(segment.meta.name, i++);

      if (IRS_UNLIKELY(!it.second)) {
        it.first->second = kInvalidCandidate;  // treat collisions as invalid
      }
    }
  }

  const auto& segments = index_meta.segments;

  ReadersType readers(segments.size());
  auto reader = readers.begin();

  for (const auto& [filename, meta] : segments) {
    const auto it = reuse_candidates.find(meta.name);

    if (it != reuse_candidates.end() && it->second != kInvalidCandidate &&
        meta == cached->meta_.index_meta.segments[it->second].meta) {
      auto tmp = (*cached)[it->second];
      IRS_ASSERT(tmp != nullptr);
      IRS_ASSERT(tmp->Meta() ==
                 cached->meta_.index_meta.segments[it->second].meta);
      *reader = std::move(tmp);
      reuse_candidates.erase(it);
    } else {
      *reader = SegmentReader{dir, meta, opts};
    }

    if (!*reader) {
      throw index_error{absl::StrCat("While opening reader for segment '",
                                     meta.name,
                                     "', error: failed to open reader")};
    }

    ++reader;
  }

  return std::make_shared<DirectoryReaderImpl>(
    dir, std::move(codec), opts,
    DirectoryMeta{.filename = *meta_file_ref,
                  .index_meta = std::move(index_meta)},
    std::move(readers));
}

}  // namespace irs
