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

#ifndef IRESEARCH_SORT_H
#define IRESEARCH_SORT_H

#include <vector>

#include "utils/attributes.hpp"
#include "utils/attribute_provider.hpp"
#include "utils/math_utils.hpp"
#include "utils/iterator.hpp"

NS_ROOT

struct collector;
struct data_output;
struct order_bucket;
struct index_reader;
struct sub_reader;
struct term_reader;

//////////////////////////////////////////////////////////////////////////////
/// @brief represents no boost value
//////////////////////////////////////////////////////////////////////////////
constexpr boost_t no_boost() noexcept { return 1.f; }

//////////////////////////////////////////////////////////////////////////////
/// @class filter_boost
/// @brief represents an addition to score from filter specific to a particular
///        document. May vary from document to document.
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API filter_boost final : attribute {
  static constexpr string_ref type_name() noexcept {
    return "iresearch::filter_boost";
  }

  boost_t value{no_boost()};
}; // filter_boost

////////////////////////////////////////////////////////////////////////////////
/// @brief stateful object used for computing the document score based on the
///        stored state
////////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API score_ctx {
  virtual ~score_ctx() = default;
}; // score_ctx

typedef std::unique_ptr<score_ctx> score_ctx_ptr;
typedef bool(*score_less_f)(const byte_type* lhs, const byte_type* rhs);
typedef void(*score_f)(const score_ctx* ctx, byte_type*);

////////////////////////////////////////////////////////////////////////////////
/// @brief combine range of scores denoted by 'src_start' and 'size' to 'dst',
///        i.e. using +=
////////////////////////////////////////////////////////////////////////////////
typedef void(*merge_f)(const order_bucket* ctx, byte_type* dst,
                       const byte_type** src, size_t count);

////////////////////////////////////////////////////////////////////////////////
/// @class sort
/// @brief base class for all user-side sort entries
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API sort {
 public:
  DECLARE_SHARED_PTR(sort);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief object used for collecting index statistics, for a specific matched
  ///        field, that are required by the scorer for scoring individual
  ///        documents
  //////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API field_collector {
    public:
     DECLARE_UNIQUE_PTR(field_collector);

     virtual ~field_collector() = default;

     ////////////////////////////////////////////////////////////////////////////
     /// @brief collect field related statistics, i.e. field used in the filter
     /// @param segment the segment being processed (e.g. for columnstore)
     /// @param field the field matched by the filter in the 'segment'
     /// @note called once for every field matched by a filter per each segment
     /// @note always called on each matched 'field' irrespective of if it
     ///       contains a matching 'term'
     ////////////////////////////////////////////////////////////////////////////
     virtual void collect(
       const sub_reader& segment,
       const term_reader& field) = 0;

     ////////////////////////////////////////////////////////////////////////////
     /// @brief clear collected stats
     ////////////////////////////////////////////////////////////////////////////
     virtual void reset() = 0;

     ///////////////////////////////////////////////////////////////////////////
     /// @brief collect field related statistics from a serialized
     ///        representation as produced by write(...) below
     ///////////////////////////////////////////////////////////////////////////
     virtual void collect(const bytes_ref& in) = 0;

     ///////////////////////////////////////////////////////////////////////////
     /// @brief serialize the internal data representation into 'out'
     ///////////////////////////////////////////////////////////////////////////
     virtual void write(data_output& out) const = 0;
  };

  template<typename T>
  FORCE_INLINE static T& score_cast(byte_type* score_buf) noexcept {
    assert(score_buf);
    return *reinterpret_cast<T*>(score_buf);
  }

  template<typename T>
  FORCE_INLINE static const T& score_cast(const byte_type* score_buf) noexcept {
    return score_cast<T>(const_cast<byte_type*>(score_buf));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief object used for collecting index statistics, for a specific matched
  ///        term of a field, that are required by the scorer for scoring
  ///        individual documents
  //////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API term_collector {
   public:
    DECLARE_UNIQUE_PTR(term_collector);

    virtual ~term_collector() = default;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief collect term related statistics, i.e. term used in the filter
    /// @param segment the segment being processed (e.g. for columnstore)
    /// @param field the field matched by the filter in the 'segment'
    /// @param term_attributes the attributes of the matched term in the field
    /// @note called once for every term matched by a filter in the 'field'
    ///       per each segment
    /// @note only called on a matched 'term' in the 'field' in the 'segment'
    ////////////////////////////////////////////////////////////////////////////
    virtual void collect(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_provider& term_attrs) = 0;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief clear collected stats
    ////////////////////////////////////////////////////////////////////////////
    virtual void reset() = 0;

    ///////////////////////////////////////////////////////////////////////////
    /// @brief collect term related statistics from a serialized
    ///        representation as produced by write(...) below
    ///////////////////////////////////////////////////////////////////////////
    virtual void collect(const bytes_ref& in) = 0;

    ///////////////////////////////////////////////////////////////////////////
    /// @brief serialize the internal data representation into 'out'
    ///////////////////////////////////////////////////////////////////////////
    virtual void write(data_output& out) const = 0;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @enum MergeType
  /// @brief possible variants of merging multiple scores
  ////////////////////////////////////////////////////////////////////////////////
  enum class MergeType {
    //////////////////////////////////////////////////////////////////////////////
    /// @brief aggregate multiple scores
    //////////////////////////////////////////////////////////////////////////////
    AGGREGATE = 0,

    //////////////////////////////////////////////////////////////////////////////
    /// @brief find max among multiple scores
    //////////////////////////////////////////////////////////////////////////////
    MAX,
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @class sort::prepared
  /// @brief base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared {
   public:
    DECLARE_UNIQUE_PTR(prepared);

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief default noop merge function
    ////////////////////////////////////////////////////////////////////////////////
    static void noop_merge(
        const order_bucket*, byte_type*,
        const byte_type**, size_t) noexcept {
      // NOOP
    }

    //////////////////////////////////////////////////////////////////////////////
    /// @brief helper function retuns merge function by a specified type
    //////////////////////////////////////////////////////////////////////////////
    template<MergeType type>
    constexpr static merge_f merge_func(const prepared& bucket) noexcept {
      switch (type) {
        case MergeType::AGGREGATE:
          return bucket.aggregate_func();
        case MergeType::MAX:
          return bucket.max_func();
        default:
          assert(false);
          return noop_merge;
      }
    }

    explicit prepared(
        merge_f aggregate_func = &noop_merge,
        merge_f max_func = &noop_merge) noexcept
      : aggregate_func_(aggregate_func),
        max_func_(max_func) {
    }

    virtual ~prepared() = default;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief store collected index statistics into 'stats' of the
    ///        current 'filter'
    /// @param filter_attrs out-parameter to store statistics for later use in
    ///        calls to score(...)
    /// @param index the full index to collect statistics on
    /// @param field the field level statistics collector as returned from
    ///        prepare_field_collector()
    /// @param term the term level statistics collector as returned from
    ///        prepare_term_collector()
    /// @note called once on the 'index' for every field+term matched by the
    ///       filter
    /// @note called once on the 'index' for every field with null term stats
    ///       if term is not applicable, e.g. by_column_existence filter
    /// @note called exactly once if field/term collection is not applicable,
    ///       e.g. collecting statistics over the columnstore
    /// @note called after all calls to collector::collect(...) on each segment
    ////////////////////////////////////////////////////////////////////////////
    virtual void collect(
      byte_type* stats,
      const index_reader& index,
      const field_collector* field,
      const term_collector* term
    ) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief the features required for proper operation of this sort::prepared
    ////////////////////////////////////////////////////////////////////////////////
    virtual const flags& features() const = 0;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create an object to be used for collecting index statistics, one
    ///        instance per matched field
    /// @return nullptr == no statistics collection required
    ////////////////////////////////////////////////////////////////////////////
    virtual field_collector::ptr prepare_field_collector() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create a stateful scorer used for computation of document scores
    ////////////////////////////////////////////////////////////////////////////////
    virtual std::pair<score_ctx_ptr, score_f> prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const byte_type* stats,
      const attribute_provider& doc_attrs,
      boost_t boost) const = 0;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create an object to be used for collecting index statistics, one
    ///        instance per matched term
    /// @return nullptr == no statistics collection required
    ////////////////////////////////////////////////////////////////////////////
    virtual term_collector::ptr prepare_term_collector() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief initialize the score buffer
    ////////////////////////////////////////////////////////////////////////////////
    virtual void prepare_score(byte_type* score) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief initialize the stats buffer
    ////////////////////////////////////////////////////////////////////////////////
    virtual void prepare_stats(byte_type* stats) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compare two score containers and determine if 'lhs' < 'rhs', i.e. <
    ////////////////////////////////////////////////////////////////////////////////
    virtual bool less(const byte_type* lhs, const byte_type* rhs) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes (first) and alignment (second) required to store
    ///        the score type (i.e. sizeof(score), alignof(score))
    /// @note alignment must satisfy the following requirements:
    ///       - be a power of 2
    ///       - be less or equal than alignof(MAX_ALIGN_T))
    ////////////////////////////////////////////////////////////////////////////////
    virtual std::pair<size_t, size_t> score_size() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes (first) and alignment (first) required to store stats
    /// @note alignment must satisfy the following requirements:
    ///       - be a power of 2
    ///       - be less or equal than alignof(MAX_ALIGN_T))
    ////////////////////////////////////////////////////////////////////////////////
    virtual std::pair<size_t, size_t> stats_size() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief aggregate range of scores denoted by 'src_start' and 'size' to 'dst',
    ///        i.e. using +=
    ////////////////////////////////////////////////////////////////////////////////
    merge_f aggregate_func() const noexcept { return aggregate_func_; }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief find max score within range of scores denoted by 'src_start' and
    ///        'size' to 'dst', i.e. using std::max(...)
    ////////////////////////////////////////////////////////////////////////////////
    merge_f max_func() const noexcept { return max_func_; }

   protected:
    merge_f aggregate_func_;
    merge_f max_func_;
  }; // prepared

  explicit sort(const type_info& type) noexcept;
  virtual ~sort() = default;

  constexpr type_info::type_id type() const noexcept { return type_; }

  virtual prepared::ptr prepare() const = 0;

 private:
  type_info::type_id type_;
}; // sort

////////////////////////////////////////////////////////////////////////////////
/// @struct order_bucket
////////////////////////////////////////////////////////////////////////////////
struct order_bucket : private util::noncopyable {
  explicit order_bucket(
      sort::prepared::ptr&& bucket,
      size_t score_offset,
      size_t stats_offset,
      bool reverse)
    : bucket(std::move(bucket)),
      score_offset(score_offset),
      stats_offset(stats_offset),
      reverse(reverse) {
  }

  order_bucket(order_bucket&& rhs) noexcept
    : bucket(std::move(rhs.bucket)),
      score_offset(rhs.score_offset),
      stats_offset(rhs.stats_offset),
      reverse(rhs.reverse) {
    rhs.score_offset = 0;
    rhs.stats_offset = 0;
  }

  order_bucket& operator=(order_bucket&& rhs) noexcept {
    if (this != &rhs) {
      bucket = std::move(rhs.bucket);
      score_offset = rhs.score_offset;
      rhs.score_offset = 0;
      stats_offset = rhs.stats_offset;
      rhs.stats_offset = 0;
      reverse = rhs.reverse;
    }
    return *this;
  }

  sort::prepared::ptr bucket; // prepared score
  size_t score_offset; // offset in score buffer
  size_t stats_offset; // offset in stats buffer
  bool reverse;
}; // order_bucket

////////////////////////////////////////////////////////////////////////////////
/// @struct score_traits
////////////////////////////////////////////////////////////////////////////////
template<typename ScoreType>
struct score_traits {
  typedef ScoreType score_type;

  FORCE_INLINE static const score_type& score_cast(const byte_type* buf) noexcept {
    assert(buf);
    return *reinterpret_cast<const score_type*>(buf);
  }

  FORCE_INLINE static score_type& score_cast(byte_type* buf) noexcept {
    return const_cast<score_type&>(score_cast(const_cast<const byte_type*>(buf)));
  }

  static void aggregate(const order_bucket* ctx, byte_type* dst,
                        const byte_type** src_begin, size_t size) {
    const auto offset = ctx->score_offset;
    auto& casted_dst = score_cast(dst + offset);
    casted_dst = ScoreType();

    const auto** src_end = src_begin + size;
    const auto** src_next = src_begin + 4;
    for (; src_next <= src_end; src_begin = src_next, src_next += 4) {
      casted_dst += score_cast(src_begin[0] + offset)
                 +  score_cast(src_begin[1] + offset)
                 +  score_cast(src_begin[2] + offset)
                 +  score_cast(src_begin[3] + offset);
    }

    switch (std::distance(src_end, src_next)) {
      case 0:
        break;
      case 1:
        casted_dst += score_cast(src_begin[0] + offset)
                   +  score_cast(src_begin[1] + offset)
                   +  score_cast(src_begin[2] + offset);
        break;
      case 2:
        casted_dst += score_cast(src_begin[0] + offset)
                   +  score_cast(src_begin[1] + offset);
        break;
      case 3:
        casted_dst += score_cast(src_begin[0] + offset);
        break;
    }
  }

  static void max(const order_bucket* ctx, byte_type* dst,
                  const byte_type** src_begin, size_t size) {
    const auto offset = ctx->score_offset;
    auto& casted_dst = score_cast(dst + offset);

    switch (size) {
      case 0:
        casted_dst = ScoreType();
        break;
      case 1:
        casted_dst = score_cast(src_begin[0] + offset);
        break;
      case 2:
        casted_dst = std::max(score_cast(src_begin[0] + offset),
                              score_cast(src_begin[1] + offset));
        break;
      default:
        casted_dst = score_cast(*src_begin + offset);
        const auto* src_end = src_begin + size;
        for (++src_begin; src_begin != src_end; ) {
          casted_dst = std::max(score_cast(*src_begin++ + offset), casted_dst);
        }
        break;
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @class prepared_sort_base
/// @brief template score for base class for all prepared(compiled) sort entries
////////////////////////////////////////////////////////////////////////////////
template<typename ScoreType,
         typename StatsType,
         typename TraitsType = score_traits<ScoreType>
> class prepared_sort_base : public sort::prepared {
 public:
  typedef TraitsType traits_t;
  typedef typename traits_t::score_type score_t;
  typedef StatsType stats_t;

  FORCE_INLINE static const stats_t& stats_cast(const byte_type* buf) noexcept {
    assert(buf);
    return *reinterpret_cast<const stats_t*>(buf);
  }

  FORCE_INLINE static stats_t& stats_cast(byte_type* buf) noexcept {
    return const_cast<stats_t&>(stats_cast(const_cast<const byte_type*>(buf)));
  }

  prepared_sort_base() noexcept
    : sort::prepared(&traits_t::aggregate, &traits_t::max) {
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief number of bytes required to store the score type (i.e. sizeof(score))
  ////////////////////////////////////////////////////////////////////////////////
  virtual inline std::pair<size_t, size_t> score_size() const noexcept final {
    static_assert(
      alignof(score_t) <= alignof(std::max_align_t),
      "alignof(score_t) must be <= alignof(std::max_align_t)"
    );

    static_assert (
      math::is_power2(alignof(score_t)),
      "alignof(score_t) must be a power of 2"
    );

    return std::make_pair(sizeof(score_t), alignof(score_t));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief number of bytes required to store stats
  ////////////////////////////////////////////////////////////////////////////////
  virtual inline std::pair<size_t, size_t> stats_size() const noexcept final {
    static_assert(
      alignof(stats_t) <= alignof(std::max_align_t),
      "alignof(stats_t) must be <= alignof(std::max_align_t)"
    );

    static_assert (
      math::is_power2(alignof(stats_t)),
      "alignof(stats_t) must be a power of 2"
    );

    return std::make_pair(sizeof(stats_t), alignof(stats_t));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the stats buffer
  ////////////////////////////////////////////////////////////////////////////////
  virtual void prepare_stats(byte_type* stats) const override final {
    stats_cast(stats) = StatsType();
  }
}; // prepared_sort_base

////////////////////////////////////////////////////////////////////////////////
/// @brief template score for base class for all prepared(compiled) sort entries
////////////////////////////////////////////////////////////////////////////////
template <typename ScoreType, typename TraitsType>
class prepared_sort_base<ScoreType, void, TraitsType> : public sort::prepared {
 public:
  typedef TraitsType traits_t;
  typedef typename traits_t::score_type score_t;
  typedef void stats_t;

  prepared_sort_base() noexcept
    : sort::prepared(&traits_t::aggregate, &traits_t::max) {
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief create an object to be used for collecting index statistics, one
  ///        instance per matched field
  /// @return nullptr == no statistics collection required
  ////////////////////////////////////////////////////////////////////////////
  virtual sort::field_collector::ptr prepare_field_collector() const override {
    return nullptr;
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief create an object to be used for collecting index statistics, one
  ///        instance per matched term
  /// @return nullptr == no statistics collection required
  ////////////////////////////////////////////////////////////////////////////
  virtual sort::term_collector::ptr prepare_term_collector() const override {
    return nullptr;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the stats buffer
  ////////////////////////////////////////////////////////////////////////////////
  virtual void prepare_stats(byte_type* /*stats*/) const override {
    // NOOP
  }

  ////////////////////////////////////////////////////////////////////////////
  /// @brief store collected index statistics into 'stats' of the
  ///        current 'filter'
  /// @param filter_attrs out-parameter to store statistics for later use in
  ///        calls to score(...)
  /// @param index the full index to collect statistics on
  /// @param field the field level statistics collector as returned from
  ///        prepare_field_collector()
  /// @param term the term level statistics collector as returned from
  ///        prepare_term_collector()
  /// @note called once on the 'index' for every field+term matched by the
  ///       filter
  /// @note called once on the 'index' for every field with null term stats
  ///       if term is not applicable, e.g. by_column_existence filter
  /// @note called exactly once if field/term collection is not applicable,
  ///       e.g. collecting statistics over the columnstore
  /// @note called after all calls to collector::collect(...) on each segment
  ////////////////////////////////////////////////////////////////////////////
  virtual void collect(
      byte_type*,
      const index_reader&,
      const sort::field_collector*,
      const sort::term_collector*) const override {
    // NOOP
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief number of bytes required to store the score type (i.e. sizeof(score))
  ////////////////////////////////////////////////////////////////////////////////
  virtual inline std::pair<size_t, size_t> score_size() const noexcept final {
    return std::make_pair(sizeof(score_t), alignof(score_t));
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief number of bytes required to store stats
  ////////////////////////////////////////////////////////////////////////////////
  virtual inline std::pair<size_t, size_t> stats_size() const noexcept final {
    return std::make_pair(size_t(0), size_t(0));
  }
}; // prepared_sort_base

////////////////////////////////////////////////////////////////////////////////
/// @brief template score for base class for basic
///        prepared(compiled) sort entries
////////////////////////////////////////////////////////////////////////////////
template<typename ScoreType,
         typename StatsType = void,
         typename TraitsType = score_traits<ScoreType>
> class prepared_sort_basic : public prepared_sort_base<ScoreType, StatsType> {
  typedef TraitsType traits_t;
  typedef prepared_sort_base<ScoreType, StatsType> base_t;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initialize the score buffer
  ////////////////////////////////////////////////////////////////////////////////
  virtual inline void prepare_score(byte_type* score) const override final {
    traits_t::score_cast(score) = ScoreType();
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief compare two score containers and determine if 'lhs' < 'rhs', i.e. <
  ////////////////////////////////////////////////////////////////////////////////
  virtual inline bool less(const byte_type* lhs,
                           const byte_type* rhs) const override final {
    return traits_t::score_cast(lhs) < traits_t::score_cast(rhs);
  }
}; // prepared_basic

////////////////////////////////////////////////////////////////////////////////
/// @class sort
/// @brief base class for all user-side sort entries
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API order final {
 public:
  class entry : private util::noncopyable {
   public:
    entry(const irs::sort::ptr& sort, bool reverse) noexcept
      : sort_(sort), reverse_(reverse) {
      assert(sort_);
    }

    entry(irs::sort::ptr&& sort, bool reverse) noexcept
      : sort_(std::move(sort)), reverse_(reverse) {
      assert(sort_);
    }

    entry(entry&& rhs) noexcept
      : sort_(std::move(rhs.sort_)), reverse_(rhs.reverse_) {
    }

    entry& operator=(entry&& rhs) noexcept {
      if (this != &rhs) {
        sort_ = std::move(rhs.sort_);
        reverse_ = rhs.reverse_;
      }
      return *this;
    }

    const irs::sort& sort() const noexcept {
      assert(sort_);
      return *sort_;
    }

    template<typename T>
    const T& sort_cast() const noexcept {
      typedef typename std::enable_if<
        std::is_base_of<irs::sort, T>::value, T
      >::type type;

#ifdef IRESEARCH_DEBUG
      return dynamic_cast<const type&>(sort());
#else
      return static_cast<const type&>(sort());
#endif // IRESEARCH_DEBUG
    }

    template<typename T>
    const T* sort_safe_cast() const noexcept {
      typedef typename std::enable_if<
        std::is_base_of<irs::sort, T>::value, T
      >::type type;

      return type::type() == sort().type()
        ? static_cast<const type*>(&sort())
        : nullptr;
    }

    bool reverse() const noexcept {
      return reverse_;
    }

   private:
    friend class order;

    irs::sort& sort() noexcept {
      assert(sort_);
      return *sort_;
    }

    sort::ptr sort_;
    bool reverse_;
  }; // entry

  typedef std::vector<entry> order_t;
  typedef order_t::const_iterator const_iterator;
  typedef order_t::iterator iterator;

  ////////////////////////////////////////////////////////////////////////////////
  /// @class sort
  /// @brief base class for all compiled sort entries
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared final : private util::noncopyable {
   public:
    typedef std::vector<order_bucket> prepared_order_t;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief a convinience class for doc_iterators to invoke scorer functions
    ///        on scorers in each order bucket
    ////////////////////////////////////////////////////////////////////////////
    class IRESEARCH_API scorers: private util::noncopyable { // noncopyable required by MSVC
     public:
      struct entry {
        entry(score_ctx_ptr&& ctx, score_f func, size_t offset)
          : ctx(std::move(ctx)),
            func(func),
            offset(offset) {
        }

        score_ctx_ptr ctx;
        score_f func;
        size_t offset;
      };

      scorers() = default;
      scorers(
        const prepared_order_t& buckets,
        const sub_reader& segment,
        const term_reader& field,
        const byte_type* stats,
        const attribute_provider& doc,
        boost_t boost);
      scorers(scorers&& other) noexcept; // function definition explicitly required by MSVC

      scorers& operator=(scorers&& other) noexcept; // function definition explicitly required by MSVC

      void score(byte_type* score) const;

      const entry& operator[](size_t i) const noexcept {
        assert(i < scorers_.size());
        return scorers_[i];
      }

      entry& operator[](size_t i) noexcept {
        assert(i < scorers_.size());
        return scorers_[i];
      }

      size_t size() const noexcept {
        return scorers_.size();
      }

     private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      std::vector<entry> scorers_; // scorer + offset
      IRESEARCH_API_PRIVATE_VARIABLES_END
    }; // scorers

    ////////////////////////////////////////////////////////////////////////////
    /// @class merger
    /// @brief a helper class to merge scores for an order bucket
    ////////////////////////////////////////////////////////////////////////////
    class merger {
     public:
      //////////////////////////////////////////////////////////////////////////
      /// @brief merge range of scores denoted by 'src_start' and 'size' to
      ///        'dst', using a merge function
      //////////////////////////////////////////////////////////////////////////
      FORCE_INLINE void operator()(
          byte_type* score,
          const byte_type** rhs_start,
          const size_t count) const {
        assert(merge_func_);
        (*merge_func_)(bucket_, score, rhs_start, count);
      }

      FORCE_INLINE bool operator==(merge_f merge_func) const noexcept {
        return merge_func == merge_func_;
      }

      FORCE_INLINE bool operator!=(merge_f merge_func) const noexcept {
        return merge_func != merge_func_;
      }

     private:
      friend class order;

      merger() = default;

      merger(const order_bucket* bucket, merge_f merge_func) noexcept
        : bucket_(bucket), merge_func_(merge_func) {
        assert(merge_func_);
      }

      const order_bucket* bucket_{nullptr};
      merge_f merge_func_{&sort::prepared::noop_merge};
    }; // merger

    static const prepared& unordered();

    prepared() = default;
    prepared(prepared&& rhs) noexcept;

    prepared& operator=(prepared&& rhs) noexcept;

    const flags& features() const noexcept { return features_; }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store the score types of all buckets
    ////////////////////////////////////////////////////////////////////////////
    size_t score_size() const noexcept { return score_size_; }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store stats of all buckets
    ////////////////////////////////////////////////////////////////////////////
    size_t stats_size() const noexcept { return stats_size_; }

    ////////////////////////////////////////////////////////////////////////////
    /// @returns number of bucket in order
    ////////////////////////////////////////////////////////////////////////////
    size_t size() const noexcept { return order_.size(); }

    ////////////////////////////////////////////////////////////////////////////
    /// @returns true if order contains no buckets
    ////////////////////////////////////////////////////////////////////////////
    bool empty() const noexcept { return order_.empty(); }

    prepared_order_t::const_iterator begin() const noexcept {
      return prepared_order_t::const_iterator(order_.begin());
    }

    prepared_order_t::const_iterator end() const noexcept {
      return prepared_order_t::const_iterator(order_.end());
    }

    const order_bucket& front() const noexcept {
      assert(!order_.empty());
      return order_.front();
    }

    const order_bucket& back() const noexcept {
      assert(!order_.empty());
      return order_.back();
    }

    const order_bucket& operator[](size_t i) const noexcept {
      return order_[i];
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief prepare empty collectors, i.e. call collect(...) on each of the
    ///        buckets without explicitly collecting field or term statistics,
    ///        e.g. for 'all' filter
    ////////////////////////////////////////////////////////////////////////////
    void prepare_collectors(byte_type* stats, const index_reader& index) const;

    ////////////////////////////////////////////////////////////////////////////
    /// @return merger object to combine multiple scores
    ////////////////////////////////////////////////////////////////////////////
    merger prepare_merger(sort::MergeType type) const noexcept {
      switch (type) {
        case sort::MergeType::AGGREGATE:
          return prepare_merger<sort::MergeType::AGGREGATE>();
        case sort::MergeType::MAX:
          return prepare_merger<sort::MergeType::MAX>();
        default:
          assert(false);
          return {};
      }
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @return set of prepared scorer objects
    ////////////////////////////////////////////////////////////////////////////
    prepared::scorers prepare_scorers(
        const sub_reader& segment,
        const term_reader& field,
        const byte_type* stats_buf,
        const attribute_provider& doc,
        irs::boost_t boost) const {
      return scorers(order_, segment, field, stats_buf, doc, boost);
    }

    bool less(const byte_type* lhs, const byte_type* rhs) const;
    void add(byte_type* lhs, const byte_type* rhs) const;
    void prepare_score(byte_type* score) const;
    void prepare_stats(byte_type* stats) const;

    template<typename T>
    constexpr const T& get(const byte_type* score, size_t i) const noexcept {
      // MacOS can't handle asserts in non-debug constexpr functions
      #if !defined(__APPLE__) && defined(IRESEARCH_DEBUG)
        assert(sizeof(T) == order_[i].bucket->score_size().first);
        assert(alignof(T) == order_[i].bucket->score_size().second);
      #endif
      return reinterpret_cast<const T&>(*(score + order_[i].score_offset));
    }

    template<
      typename StringType,
      typename TraitsType = typename StringType::traits_type
    > constexpr StringType to_string(const byte_type* score, size_t i) const {
      typedef typename TraitsType::char_type char_type;

      return StringType(
        reinterpret_cast<const char_type*>(score + order_[i].score_offset),
        order_[i].bucket->score_size()
      );
    }

  private:
    friend class order;

    template<typename Func>
    void for_each(const Func& func) const {
      std::for_each(order_.begin(), order_.end(), func);
    }

    template<sort::MergeType Type>
    merger prepare_merger() const noexcept {
      switch (order_.size()) {
        case 0: return { };
        case 1: return {
            &order_[0],
            sort::prepared::merge_func<Type>(*order_[0].bucket)
          };
        case 2: return {
            &order_[0],
            [](const order_bucket* ctx, byte_type* dst,
               const byte_type** src_start, const size_t size) {
                sort::prepared::merge_func<Type>(*ctx[0].bucket)(ctx,     dst, &(*src_start), size);
                sort::prepared::merge_func<Type>(*ctx[1].bucket)(ctx + 1, dst, &(*src_start), size);
          }};
        default: return {
            reinterpret_cast<const order_bucket*>(&order_),
            [](const order_bucket* ctx, byte_type* dst,
               const byte_type** src_start, const size_t size) {
                auto& order = *reinterpret_cast<const order::prepared*>(ctx);
                order.for_each([dst, src_start, size](const order_bucket& sort) {
                  assert(sort.bucket);
                  sort::prepared::merge_func<Type>(*sort.bucket)(&sort, dst, src_start, size);
                });
          }};
      }
    }


    IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
    prepared_order_t order_;
    flags features_;
    size_t score_size_{ 0 };
    size_t stats_size_{ 0 };
    IRESEARCH_API_PRIVATE_VARIABLES_END
  }; // prepared

  static const order& unordered();

  order() = default;
  order(order&& rhs) noexcept
    : order_(std::move(rhs.order_)) {
  }

  order& operator=(order&& rhs) noexcept {
    if (this != &rhs) {
      order_ = std::move(rhs.order_);
    }

    return *this;
  }

  bool operator==(const order& other) const;

  bool operator!=(const order& other) const {
    return !(*this == other);
  }

  prepared prepare() const;

  order& add(bool reverse, sort::ptr const& sort);

  template<typename T, typename... Args>
  T& add(bool reverse, Args&&... args) {
    typedef typename std::enable_if<
      std::is_base_of<sort, T>::value, T
    >::type type;

    add(reverse, type::make(std::forward<Args>(args)...));
    return static_cast<type&>(order_.back().sort());
  }

  template<typename T>
  void remove() {
    typedef typename std::enable_if<
      std::is_base_of<sort, T>::value, T
    >::type type;

    remove(type::type());
  }

  void remove(type_info::type_id type);
  void clear() noexcept { order_.clear(); }

  size_t size() const noexcept { return order_.size(); }
  bool empty() const noexcept { return order_.empty(); }

  const_iterator begin() const noexcept { return order_.begin(); }
  const_iterator end() const noexcept { return order_.end(); }

  iterator begin() noexcept { return order_.begin(); }
  iterator end() noexcept { return order_.end(); }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  order_t order_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // order

NS_END

#endif
