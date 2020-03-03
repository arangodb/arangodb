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
#include "utils/attributes_provider.hpp"
#include "utils/math_utils.hpp"
#include "utils/iterator.hpp"

NS_ROOT

struct data_output; // forward declaration

//////////////////////////////////////////////////////////////////////////////
/// @brief represents a boost related to the particular query
//////////////////////////////////////////////////////////////////////////////
typedef float_t boost_t;

constexpr boost_t no_boost() noexcept { return 1.f; }

struct collector;
struct index_reader;
struct sub_reader;
struct term_reader;

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
       const term_reader& field
     ) = 0;

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
      const attribute_view& term_attrs
    ) = 0;

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
  /// @class sort::prepared
  /// @brief base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared {
   public:
    DECLARE_UNIQUE_PTR(prepared);

    prepared() = default;
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
      const attribute_view& doc_attrs,
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
    /// @brief merge range of scores denoted by 'src_start' and 'size' at a
    ///        specified 'offset' to 'dst', i.e. using +=
    ////////////////////////////////////////////////////////////////////////////////
    virtual void merge(byte_type* dst, const byte_type** src_start,
                       const size_t size, size_t offset) const = 0;

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

   private:
    attribute_view attrs_;
  }; // prepared

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief template score for base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  template <typename ScoreType, typename StatsType>
  class prepared_base : public prepared {
   public:
    typedef ScoreType score_t;
    typedef StatsType stats_t;

    FORCE_INLINE static const score_t& score_cast(const byte_type* buf) noexcept {
      assert(buf);
      return *reinterpret_cast<const score_t*>(buf);
    }

    FORCE_INLINE static score_t& score_cast(byte_type* buf) noexcept {
      return const_cast<score_t&>(score_cast(const_cast<const byte_type*>(buf)));
    }

    FORCE_INLINE static const stats_t& stats_cast(const byte_type* buf) noexcept {
      assert(buf);
      return *reinterpret_cast<const stats_t*>(buf);
    }

    FORCE_INLINE static stats_t& stats_cast(byte_type* buf) noexcept {
      return const_cast<stats_t&>(stats_cast(const_cast<const byte_type*>(buf)));
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store the score type (i.e. sizeof(score))
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline std::pair<size_t, size_t> score_size() const noexcept final {
      static_assert(
        alignof(score_t) <= alignof(MAX_ALIGN_T),
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
        alignof(stats_t) <= alignof(MAX_ALIGN_T),
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
  }; // prepared_base

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief template score for base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  template <typename ScoreType>
  class prepared_base<ScoreType, void> : public prepared {
   public:
    typedef ScoreType score_t;
    typedef void stats_t;

    FORCE_INLINE static const score_t& score_cast(const byte_type* score_buf) noexcept {
      assert(score_buf);
      return *reinterpret_cast<const score_t*>(score_buf);
    }

    FORCE_INLINE static score_t& score_cast(byte_type* score_buf) noexcept {
      assert(score_buf);
      return *reinterpret_cast<score_t*>(score_buf);
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create an object to be used for collecting index statistics, one
    ///        instance per matched field
    /// @return nullptr == no statistics collection required
    ////////////////////////////////////////////////////////////////////////////
    virtual field_collector::ptr prepare_field_collector() const override {
      return nullptr;
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create an object to be used for collecting index statistics, one
    ///        instance per matched term
    /// @return nullptr == no statistics collection required
    ////////////////////////////////////////////////////////////////////////////
    virtual term_collector::ptr prepare_term_collector() const override {
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
        const field_collector*,
        const term_collector*
    ) const override {
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
  }; // prepared_base

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief template score for base class for basic
  ///        prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  template <typename ScoreType, typename StatsType = void>
  class prepared_basic : public prepared_base<ScoreType, StatsType> {
    typedef prepared_base<ScoreType, StatsType> base_t;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief initialize the score buffer
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline void prepare_score(byte_type* score) const override final {
      base_t::score_cast(score) = ScoreType();
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief merge range of scores denoted by 'src_start' and 'size' at a
    ///        specified 'offset' to 'dst', i.e. using +=
    ////////////////////////////////////////////////////////////////////////////////
    virtual void merge(byte_type* dst, const byte_type** src_start,
                       const size_t size, size_t offset) const {
      auto& casted_dst = base_t::score_cast(dst + offset);
      casted_dst = ScoreType();
      for (size_t i = 0; i < size; ++i) {
        casted_dst += base_t::score_cast(src_start[i] + offset);
      }
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compare two score containers and determine if 'lhs' < 'rhs', i.e. <
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline bool less(const byte_type* lhs,
                             const byte_type* rhs) const override final {
      return base_t::score_cast(lhs) < base_t::score_cast(rhs);
    }
  }; // prepared_basic

  //////////////////////////////////////////////////////////////////////////////
  /// @class type_id
  //////////////////////////////////////////////////////////////////////////////
  class type_id: public iresearch::type_id, util::noncopyable {
   public:
    type_id(const string_ref& name): name_(name) {}
    operator const type_id*() const { return this; }
    const string_ref& name() const { return name_; }

   private:
    string_ref name_;
  }; // type_id

  explicit sort(const type_id& id) noexcept;
  virtual ~sort() = default;

  const type_id& type() const { return *type_; }

  virtual prepared::ptr prepare() const = 0;

 private:
  const type_id* type_;
}; // sort

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
    struct prepared_sort : private util::noncopyable {
      explicit prepared_sort(
          sort::prepared::ptr&& bucket,
          size_t score_offset,
          size_t stats_offset,
          bool reverse)
        : bucket(std::move(bucket)),
          score_offset(score_offset),
          stats_offset(stats_offset),
          reverse(reverse) {
      }

      prepared_sort(prepared_sort&& rhs) noexcept
        : bucket(std::move(rhs.bucket)),
          score_offset(rhs.score_offset),
          stats_offset(rhs.stats_offset),
          reverse(rhs.reverse) {
        rhs.score_offset = 0;
        rhs.stats_offset = 0;
      }

      prepared_sort& operator=(prepared_sort&& rhs) noexcept {
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

      sort::prepared::ptr bucket;
      size_t score_offset; // offset in score buffer
      size_t stats_offset; // offset in stats buffer
      bool reverse;
    }; // prepared_sort

    typedef std::vector<prepared_sort> prepared_order_t;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief a convinience class for filters to invoke collector functions
    ///        on collectors in each order bucket
    ////////////////////////////////////////////////////////////////////////////
    class IRESEARCH_API collectors: private util::noncopyable { // noncopyable required by MSVC
     public:
      collectors(const prepared& buckets, size_t terms_count);
      collectors(collectors&& other) noexcept; // function definition explicitly required by MSVC

      //////////////////////////////////////////////////////////////////////////
      /// @brief collect field related statistics, i.e. field used in the filter
      /// @param segment the segment being processed (e.g. for columnstore)
      /// @param field the field matched by the filter in the 'segment'
      /// @note called once for every field matched by a filter per each segment
      /// @note always called on each matched 'field' irrespective of if it
      ///       contains a matching 'term'
      //////////////////////////////////////////////////////////////////////////
      void collect(const sub_reader& segment, const term_reader& field) const;

      //////////////////////////////////////////////////////////////////////////
      /// @brief collect term related statistics, i.e. term used in the filter
      /// @param segment the segment being processed (e.g. for columnstore)
      /// @param field the field matched by the filter in the 'segment'
      /// @param term_offset offset of term, value < constructor 'terms_count'
      /// @param term_attributes the attributes of the matched term in the field
      /// @note called once for every term matched by a filter in the 'field'
      ///       per each segment
      /// @note only called on a matched 'term' in the 'field' in the 'segment'
      //////////////////////////////////////////////////////////////////////////
      void collect(
        const sub_reader& segment,
        const term_reader& field,
        size_t term_offset,
        const attribute_view& term_attrs
      ) const;

      //////////////////////////////////////////////////////////////////////////
      /// @brief store collected index statistics into 'stats' of the
      ///        current 'filter'
      /// @param stats out-parameter to store statistics for later use in
      ///        calls to score(...)
      /// @param index the full index to collect statistics on
      /// @note called once on the 'index' for every term matched by a filter
      ///       calling collect(...) on each of its segments
      /// @note if not matched terms then called exactly once
      //////////////////////////////////////////////////////////////////////////
      void finish(byte_type* stats, const index_reader& index) const;

      //////////////////////////////////////////////////////////////////////////
      /// @brief add collectors for another term
      /// @return term_offset
      //////////////////////////////////////////////////////////////////////////
      size_t push_back();

     private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      const std::vector<prepared_sort>& buckets_;
      std::vector<sort::field_collector::ptr> field_collectors_; // size == buckets_.size()
      std::vector<sort::term_collector::ptr> term_collectors_; // size == buckets_.size() * terms_count, layout order [t0.b0, t0.b1, ... t0.bN, t1.b0, t1.b1 ... tM.BN]
      IRESEARCH_API_PRIVATE_VARIABLES_END
    };

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
        const attribute_view& doc,
        boost_t boost
      );
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
    bool empty() const noexcept { return order_.empty(); }

    prepared_order_t::const_iterator begin() const noexcept {
      return prepared_order_t::const_iterator(order_.begin());
    }

    prepared_order_t::const_iterator end() const noexcept {
      return prepared_order_t::const_iterator(order_.end());
    }

    const prepared_sort& operator[](size_t i) const noexcept {
      return order_[i];
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create an index statistics compound collector for all buckets
    /// @param terms_count number of term_collectors to allocate
    ///        0 == collect only field level statistics e.g. by_column_existence
    ////////////////////////////////////////////////////////////////////////////
    collectors prepare_collectors(size_t terms_count = 0) const {
      return collectors(*this, terms_count);
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief prepare empty collectors, i.e. call collect(...) on each of the
    ///        buckets without explicitly collecting field or term statistics,
    ///        e.g. for 'all' filter
    ////////////////////////////////////////////////////////////////////////////
    void prepare_collectors(byte_type* stats, const index_reader& index) const;

    prepared::scorers prepare_scorers(
        const sub_reader& segment,
        const term_reader& field,
        const byte_type* stats_buf,
        const attribute_view& doc,
        irs::boost_t boost
    ) const {
      return scorers(order_, segment, field, stats_buf, doc, boost);
    }

    bool less(const byte_type* lhs, const byte_type* rhs) const;
    void add(byte_type* lhs, const byte_type* rhs) const;
    void prepare_score(byte_type* score) const;
    void prepare_stats(byte_type* stats) const;

    void merge(byte_type* score, const byte_type** rhs_start, size_t count) const {
      switch (order_.size()) {
        case 1:
          order_[0].bucket->merge(score, &(*rhs_start), count, order_[0].score_offset);
          break;
        case 2:
          order_[0].bucket->merge(score, &(*rhs_start), count, order_[0].score_offset);
          order_[1].bucket->merge(score, &(*rhs_start), count, order_[1].score_offset);
          break;
        default:
          for_each([score, &rhs_start, count](const prepared_sort& sort) {
            assert(sort.bucket);
            sort.bucket->merge(score, &(*rhs_start), count, sort.score_offset);
          });
      }
    }

    template<typename T>
    constexpr const T& get(const byte_type* score, size_t i) const noexcept {
      #if !defined(__APPLE__) && defined(IRESEARCH_DEBUG) // MacOS can't handle asserts in non-debug constexpr functions
        assert(sizeof(T) == order_[i].bucket->score_size().first);
        assert(alignof(T) == order_[i].bucket->score_size().second);
      #endif
      return reinterpret_cast<const T&>(*(score + order_[i].score_offset));
    }

    template<
      typename StringType,
      typename TraitsType = typename StringType::traits_type
    > constexpr StringType to_string(
        const byte_type* score, size_t i
    ) const {
      typedef typename TraitsType::char_type char_type;

      return StringType(
        reinterpret_cast<const char_type*>(score + order_[i].score_offset),
        order_[i].bucket->score_size()
      );
    }

  private:
    friend class order;

    template<typename Func>
    inline void for_each(const Func& func) const {
      std::for_each(order_.begin(), order_.end(), func);
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

  void remove(const type_id& id);
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

//////////////////////////////////////////////////////////////////////////////
/// @class filter_boost
/// @brief represents an addition to score from filter specific to a particular 
///        document. May vary from document to document.
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API filter_boost : public basic_attribute<boost_t> {
  DECLARE_ATTRIBUTE_TYPE();
  filter_boost() noexcept;

  void clear() { value = 1.f; }
};


NS_END

#endif
