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

#include "composite_reader_impl.hpp"
#include "utils/directory_utils.hpp"
#include "utils/singleton.hpp"
#include "utils/string_utils.hpp"
#include "utils/type_limits.hpp"

#include "directory_reader.hpp"
#include "segment_reader.hpp"

NS_LOCAL

MSVC_ONLY(__pragma(warning(push)))
MSVC_ONLY(__pragma(warning(disable:4457))) // variable hides function param
irs::index_file_refs::ref_t load_newest_index_meta(
  irs::index_meta& meta,
  const irs::directory& dir,
  const irs::format* codec
) NOEXCEPT {
  // if a specific codec was specified
  if (codec) {
    try {
      auto reader = codec->get_index_meta_reader();

      if (!reader) {
        return nullptr;
      }

      irs::index_file_refs::ref_t ref;
      std::string filename;

      // ensure have a valid ref to a filename
      while (!ref) {
        const bool index_exists = reader->last_segments_file(dir, filename);

        if (!index_exists) {
          return nullptr;
        }

        ref = irs::directory_utils::reference(
          const_cast<irs::directory&>(dir), filename
        );
      }

      if (ref) {
        reader->read(dir, meta, *ref);
      }

      return ref;
    } catch (...) {
      IR_LOG_EXCEPTION();

      return nullptr;
    }
  }

  std::unordered_set<irs::string_ref> codecs;
  auto visitor = [&codecs](const irs::string_ref& name)->bool {
    codecs.insert(name);
    return true;
  };

  if (!irs::formats::visit(visitor)) {
    return nullptr;
  }

  struct {
    std::time_t mtime;
    irs::index_meta_reader::ptr reader;
    irs::index_file_refs::ref_t ref;
  } newest;

  newest.mtime = (irs::integer_traits<time_t>::min)();

  try {
    for (auto& name: codecs) {
      auto codec = irs::formats::get(name);

      if (!codec) {
        continue; // try the next codec
      }

      auto reader = codec->get_index_meta_reader();

      if (!reader) {
        continue; // try the next codec
      }

      irs::index_file_refs::ref_t ref;
      std::string filename;

      // ensure have a valid ref to a filename
      while (!ref) {
        const bool index_exists = reader->last_segments_file(dir, filename);

        if (!index_exists) {
          break; // try the next codec
        }

        ref = irs::directory_utils::reference(
          const_cast<irs::directory&>(dir), filename
        );
      }

      // initialize to a value that will never pass 'if' below (to make valgrind happy)
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
  } catch (...) {
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}
MSVC_ONLY(__pragma(warning(pop)))

NS_END

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @brief context specialization for segment_reader
////////////////////////////////////////////////////////////////////////////////

template<>
struct context<segment_reader> {
  segment_reader reader;
  doc_id_t base = 0; // min document id
  doc_id_t max = 0; // max document id

  operator doc_id_t() const { return max; }
};

// -------------------------------------------------------------------
// directory_reader
// -------------------------------------------------------------------

class directory_reader_impl :
  public composite_reader_impl<segment_reader> {
 public:
  DECLARE_SHARED_PTR(directory_reader_impl); // required for NAMED_PTR(...)

  const directory& dir() const NOEXCEPT {
    return dir_;
  }

  // open a new directory reader
  // if codec == nullptr then use the latest file for all known codecs
  // if cached != nullptr then try to reuse its segments
  static index_reader::ptr open(
    const directory& dir,
    const format* codec = nullptr,
    const index_reader::ptr& cached = nullptr
  );

 private:
  typedef std::unordered_set<index_file_refs::ref_t> segment_file_refs_t;
  typedef std::vector<segment_file_refs_t> reader_file_refs_t;

  const directory& dir_;
  reader_file_refs_t file_refs_;

  directory_reader_impl(
    const directory& dir,
    reader_file_refs_t&& file_refs,
    index_meta&& meta,
    ctxs_t&& ctxs,
    uint64_t docs_count,
    uint64_t docs_max
  );
}; // directory_reader_impl

directory_reader::directory_reader(impl_ptr&& impl) NOEXCEPT
  : impl_(std::move(impl)) {
}

directory_reader::directory_reader(const directory_reader& other) NOEXCEPT {
  *this = other;
}

directory_reader& directory_reader::operator=(
    const directory_reader& other) NOEXCEPT {
  if (this != &other) {
    // make a copy
    impl_ptr impl = atomic_utils::atomic_load(&other.impl_);

    atomic_utils::atomic_store(&impl_, impl);
  }

  return *this;
}

/*static*/ directory_reader directory_reader::open(
    const directory& dir,
    format::ptr codec /*= nullptr*/) {
  return directory_reader_impl::open(dir, codec.get());
}

directory_reader directory_reader::reopen(
    format::ptr codec /*= nullptr*/) const {
  // make a copy
  impl_ptr impl = atomic_utils::atomic_load(&impl_);

#ifdef IRESEARCH_DEBUG
  auto& reader_impl = dynamic_cast<const directory_reader_impl&>(*impl);
#else
  auto& reader_impl = static_cast<const directory_reader_impl&>(*impl);
#endif

  return directory_reader_impl::open(
    reader_impl.dir(), codec.get(), impl
  );
}

// -------------------------------------------------------------------
// directory_reader_impl
// -------------------------------------------------------------------

directory_reader_impl::directory_reader_impl(
    const directory& dir,
    reader_file_refs_t&& file_refs,
    index_meta&& meta,
    ctxs_t&& ctxs,
    uint64_t docs_count,
    uint64_t docs_max)
  : composite_reader_impl(std::move(meta), std::move(ctxs), docs_count, docs_max),
    dir_(dir),
    file_refs_(std::move(file_refs)) {
}

/*static*/ index_reader::ptr directory_reader_impl::open(
    const directory& dir,
    const format* codec /*= nullptr*/,
    const index_reader::ptr& cached /*= nullptr*/) {
  index_meta meta;
  index_file_refs::ref_t meta_file_ref = load_newest_index_meta(meta, dir, codec);

  if (!meta_file_ref) {
    throw index_not_found();
  }

#ifdef IRESEARCH_DEBUG
  auto* cached_impl = dynamic_cast<const directory_reader_impl*>(cached.get());
  assert(!cached || cached_impl);
#else
  auto* cached_impl = static_cast<const directory_reader_impl*>(cached.get());
#endif

  if (cached_impl && cached_impl->meta() == meta) {
    return cached; // no changes to refresh
  }

  const auto INVALID_CANDIDATE = integer_traits<size_t>::const_max;
  std::unordered_map<string_ref, size_t> reuse_candidates; // map by segment name to old segment id

  for(size_t i = 0, count = cached_impl ? cached_impl->meta().size() : 0; i < count; ++i) {
    auto itr = reuse_candidates.emplace(cached_impl->meta().segment(i).meta.name, i);

    if (!itr.second) {
      itr.first->second = INVALID_CANDIDATE; // treat collisions as invalid
    }
  }

  ctxs_t ctxs(meta.size());
  uint64_t docs_max = 0; // overall number of documents (with deleted)
  uint64_t docs_count = 0; // number of live documents
  reader_file_refs_t file_refs(ctxs.size() + 1); // +1 for index_meta file refs
  segment_file_refs_t tmp_file_refs;
  auto visitor = [&tmp_file_refs](index_file_refs::ref_t&& ref)->bool {
    tmp_file_refs.emplace(std::move(ref));
    return true;
  };

  for (size_t i = 0, size = meta.size(); i < size; ++i) {
    auto& ctx = ctxs[i];
    auto& segment = meta.segment(i).meta;
    auto& segment_file_refs = file_refs[i];
    auto itr = reuse_candidates.find(segment.name);

    if (itr != reuse_candidates.end()
        && itr->second != INVALID_CANDIDATE
        && segment == cached_impl->meta().segment(itr->second).meta) {
      ctx.reader = (*cached_impl)[itr->second].reopen(segment);
      reuse_candidates.erase(itr);
    } else {
      ctx.reader = segment_reader::open(dir, segment);
    }

    if (!ctx.reader) {
      throw index_error(string_utils::to_string(
        "while opening reader for segment '%s', error: failed to open reader",
        segment.name.c_str()
      ));
    }

    ctx.base = static_cast<doc_id_t>(docs_max);
    docs_max += ctx.reader.docs_count();
    docs_count += ctx.reader.live_docs_count();
    ctx.max = doc_id_t(type_limits<type_t::doc_id_t>::min() + docs_max - 1);
    directory_utils::reference(const_cast<directory&>(dir), segment, visitor, true);
    segment_file_refs.swap(tmp_file_refs);
  }

  directory_utils::reference(const_cast<directory&>(dir), meta, visitor, true);
  tmp_file_refs.emplace(meta_file_ref);
  file_refs.back().swap(tmp_file_refs); // use last position for storing index_meta refs

  PTR_NAMED(
    directory_reader_impl,
    reader,
    dir,
    std::move(file_refs),
    std::move(meta),
    std::move(ctxs),
    docs_count,
    docs_max
  );

  return reader;
}

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
