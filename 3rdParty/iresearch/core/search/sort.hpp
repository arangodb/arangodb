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

#include "shared.hpp"
#include "utils/attributes.hpp"
#include "utils/attributes_provider.hpp"
#include "utils/iterator.hpp"

#include <vector>

NS_ROOT

struct data_output; // forward declaration

//////////////////////////////////////////////////////////////////////////////
/// @class boost
/// @brief represents a boost related to the particular query
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API boost : basic_stored_attribute<float_t> {
  typedef float_t boost_t;

  static CONSTEXPR boost_t no_boost() NOEXCEPT { return 1.f; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief applies boost to the specified attributes collection ("src")
  //////////////////////////////////////////////////////////////////////////////
  static void apply(attribute_store& src, boost_t value) {
    if (irs::boost::no_boost() == value) {
      return;
    }

    src.emplace<irs::boost>()->value *= value;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns a value of the "boost" attribute in the specified "src"
  /// collection, or NO_BOOST value if there is no "boost" attribute in "src"
  //////////////////////////////////////////////////////////////////////////////
  static boost_t extract(const attribute_store& src) {
    boost_t value = no_boost();
    auto& attr = src.get<iresearch::boost>();

    if (attr) value = attr->value;

    return value;
  }

  DECLARE_ATTRIBUTE_TYPE();
  DECLARE_FACTORY();

  boost() NOEXCEPT;

  void clear() {
    value = no_boost();
  }
}; // boost

struct collector;
struct index_reader;
struct sub_reader;
struct term_reader;

typedef bool (*score_less_f)(const byte_type* lhs, const byte_type* rhs);

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief stateful object used for computing the document score based on the
  ///        stored state
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API scorer {
   public:
    DECLARE_UNIQUE_PTR(scorer);
    DEFINE_FACTORY_INLINE(scorer)

    virtual ~scorer() = default;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set the document score based on the stored state
    ////////////////////////////////////////////////////////////////////////////////
    virtual void score(byte_type* score_buf) = 0;
  }; // scorer

  template <typename T>
  class scorer_base : public scorer {
   public:
    typedef T score_t;

    FORCE_INLINE static T& score_cast(byte_type* score_buf) NOEXCEPT {
      assert(score_buf);
      return *reinterpret_cast<T*>(score_buf);
    }
  }; // scorer_base

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
  class IRESEARCH_API prepared : public util::attribute_view_provider {
   public:
    DECLARE_UNIQUE_PTR(prepared);

    prepared() = default;
    explicit prepared(attribute_view&& attrs) NOEXCEPT;
    virtual ~prepared() = default;

    using util::attribute_view_provider::attributes;
    virtual attribute_view& attributes() NOEXCEPT override final {
      return attrs_;
    }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief store collected index statistics into 'filter_attrs' of the
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
      attribute_store& filter_attrs,
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
    virtual scorer::ptr prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_store& query_attrs,
      const attribute_view& doc_attrs
    ) const = 0;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create an object to be used for collecting index statistics, one
    ///        instance per matched term
    /// @return nullptr == no statistics collection required
    ////////////////////////////////////////////////////////////////////////////
    virtual term_collector::ptr prepare_term_collector() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief initialize the score container and prepare it for add(...) calls
    ////////////////////////////////////////////////////////////////////////////////
    virtual void prepare_score(byte_type* score) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief add the score from 'src' to the score in 'dst', i.e. +=
    ////////////////////////////////////////////////////////////////////////////////
    virtual void add(byte_type* dst, const byte_type* src) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compare two score containers and determine if 'lhs' < 'rhs', i.e. <
    ////////////////////////////////////////////////////////////////////////////////
    virtual bool less(const byte_type* lhs, const byte_type* rhs) const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store the score type (i.e. sizeof(score))
    ////////////////////////////////////////////////////////////////////////////////
    virtual size_t size() const = 0;

   private:
    attribute_view attrs_;
  }; // prepared

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief template score for base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  template <typename T>
  class prepared_base : public prepared {
   public:
    typedef T score_t;

    FORCE_INLINE static const T& score_cast(const byte_type* score_buf) NOEXCEPT {
      assert(score_buf);
      return *reinterpret_cast<const T*>(score_buf);
    }

    FORCE_INLINE static T& score_cast(byte_type* score_buf) NOEXCEPT {
      assert(score_buf);
      return *reinterpret_cast<T*>(score_buf);
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store the score type (i.e. sizeof(score))
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline size_t size() const final override {
      return sizeof(score_t);
    }
  }; // prepared_base

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief template score for base class for basic
  ///        prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  template <typename T>
  class prepared_basic : public prepared_base<T> {
    typedef prepared_base<T> base_t;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief initialize the score container and prepare it for add(...) calls
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline void prepare_score(byte_type* score) const override final {
      base_t::score_cast(score) = T();
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief add the score from 'src' to the score in 'dst', i.e. +=
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline void add(
      byte_type* dst,
      const byte_type* src
    ) const override final {
      base_t::score_cast(dst) += base_t::score_cast(src);
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief compare two score containers and determine if 'lhs' < 'rhs', i.e. <
    ////////////////////////////////////////////////////////////////////////////////
    virtual inline bool less(
      const byte_type* lhs, const byte_type* rhs
    ) const override final {
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

  explicit sort(const type_id& id) NOEXCEPT;
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
    entry(const irs::sort::ptr& sort, bool reverse) NOEXCEPT
      : sort_(sort), reverse_(reverse) {
      assert(sort_);
    }

    entry(irs::sort::ptr&& sort, bool reverse) NOEXCEPT
      : sort_(std::move(sort)), reverse_(reverse) {
      assert(sort_);
    }

    entry(entry&& rhs) NOEXCEPT
      : sort_(std::move(rhs.sort_)), reverse_(rhs.reverse_) {
    }

    entry& operator=(entry&& rhs) NOEXCEPT {
      if (this != &rhs) {
        sort_ = std::move(rhs.sort_);
        reverse_ = rhs.reverse_;
      }
      return *this;
    }

    const irs::sort& sort() const NOEXCEPT {
      assert(sort_);
      return *sort_;
    }

    template<typename T>
    const T& sort_cast() const NOEXCEPT {
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
    const T* sort_safe_cast() const NOEXCEPT {
      typedef typename std::enable_if<
        std::is_base_of<irs::sort, T>::value, T
      >::type type;

      return type::type() == sort().type()
        ? static_cast<const type*>(&sort())
        : nullptr;
    }

    bool reverse() const NOEXCEPT {
      return reverse_;
    }

   private:
    friend class order;

    irs::sort& sort() NOEXCEPT {
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
      explicit prepared_sort(sort::prepared::ptr&& bucket, bool reverse)
        : bucket(std::move(bucket)), offset(0), reverse(reverse) {
      }

      prepared_sort(prepared_sort&& rhs) NOEXCEPT
        : bucket(std::move(rhs.bucket)),
          offset(rhs.offset),
          reverse(rhs.reverse) {
        rhs.offset = 0;
      }

      prepared_sort& operator=(prepared_sort&& rhs) NOEXCEPT {
        if (this != &rhs) {
          bucket = std::move(rhs.bucket);
          offset = rhs.offset;
          rhs.offset = 0;
          reverse = rhs.reverse;
        }
        return *this;
      }

      sort::prepared::ptr bucket;
      size_t offset;
      bool reverse;
    }; // prepared_sort

    typedef std::vector<prepared_sort> prepared_order_t;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief a convinience class for filters to invoke collector functions
    ///        on collectors in each order bucket
    ////////////////////////////////////////////////////////////////////////////
    class IRESEARCH_API collectors: private util::noncopyable { // noncopyable required by MSVC
     public:
      collectors(const prepared_order_t& buckets, size_t terms_count);
      collectors(collectors&& other) NOEXCEPT; // function definition explicitly required by MSVC

      //////////////////////////////////////////////////////////////////////////
      /// @brief collect field related statistics, i.e. field used in the filter
      /// @param segment the segment being processed (e.g. for columnstore)
      /// @param field the field matched by the filter in the 'segment'
      /// @note called once for every field matched by a filter per each segment
      /// @note always called on each matched 'field' irrespective of if it
      ///       contains a matching 'term'
      //////////////////////////////////////////////////////////////////////////
      void collect(
        const sub_reader& segment,
        const term_reader& field
      ) const;

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
      /// @brief store collected index statistics into 'filter_attrs' of the
      ///        current 'filter'
      /// @param filter_attrs out-parameter to store statistics for later use in
      ///        calls to score(...)
      /// @param index the full index to collect statistics on
      /// @note called once on the 'index' for every term matched by a filter
      ///       calling collect(...) on each of its segments
      /// @note if not matched terms then called exactly once
      //////////////////////////////////////////////////////////////////////////
      void finish(
        attribute_store& filter_attrs,
        const index_reader& index
      ) const;

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
      scorers() = default;
      scorers(
        const prepared_order_t& buckets,
        const sub_reader& segment,
        const term_reader& field,
        const attribute_store& stats,
        const attribute_view& doc
      );
      scorers(scorers&& other) NOEXCEPT; // function definition explicitly required by MSVC

      scorers& operator=(scorers&& other) NOEXCEPT; // function definition explicitly required by MSVC

      void score(const prepared& ord, byte_type* score) const;

      const std::pair<sort::scorer::ptr, size_t>& operator[](size_t i) const NOEXCEPT {
        assert(i < scorers_.size());
        return scorers_[i];
      }

      size_t size() const NOEXCEPT {
        return scorers_.size();
      }

     private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      std::vector<std::pair<sort::scorer::ptr, size_t>> scorers_; // scorer + offset
      IRESEARCH_API_PRIVATE_VARIABLES_END
    }; // scorers

    static const prepared& unordered();

    prepared();
    prepared(prepared&& rhs) NOEXCEPT;

    prepared& operator=(prepared&& rhs) NOEXCEPT;

    const flags& features() const { return features_; }

    ////////////////////////////////////////////////////////////////////////////
    /// @brief create an index statistics compound collector for all buckets
    /// @param terms_count number of term_collectors to allocate
    ///        0 == collect only field level statistics e.g. by_column_existence
    ////////////////////////////////////////////////////////////////////////////
    collectors prepare_collectors(size_t terms_count = 0) const;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief prepare empty collectors, i.e. call collect(...) on each of the
    ///        buckets without explicitly collecting field or term statistics,
    ///        e.g. for 'all' filter
    ////////////////////////////////////////////////////////////////////////////
    void prepare_collectors(
      attribute_store& filter_attrs,
      const index_reader& index
    ) const;

    ////////////////////////////////////////////////////////////////////////////
    /// @brief number of bytes required to store the score types of all buckets
    ////////////////////////////////////////////////////////////////////////////
    size_t size() const { return size_; }

    bool empty() const { return order_.empty(); }

    prepared_order_t::const_iterator begin() const {
      return prepared_order_t::const_iterator(order_.begin());
    }

    prepared_order_t::const_iterator end() const { 
      return prepared_order_t::const_iterator(order_.end());
    }

    const prepared_sort& operator[]( size_t i ) const {
      return order_[i];
    }

    prepared::scorers prepare_scorers(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_store& stats,
      const attribute_view& doc
    ) const;

    bool less(const byte_type* lhs, const byte_type* rhs) const;
    void add(byte_type* lhs, const byte_type* rhs) const;
    void prepare_score(byte_type* score) const;

    template<typename T>
    CONSTEXPR const T& get(const byte_type* score, size_t i) const NOEXCEPT {
      #if !defined(__APPLE__) && defined(IRESEARCH_DEBUG) // MacOS can't handle asserts in non-debug CONSTEXPR functions
        assert(sizeof(T) == order_[i].bucket->size());
      #endif
      return reinterpret_cast<const T&>(*(score + order_[i].offset));
    }

    template<
      typename StringType,
      typename TraitsType = typename StringType::traits_type
    > CONSTEXPR StringType to_string(
        const byte_type* score, size_t i
    ) const {
      typedef typename TraitsType::char_type char_type;

      return StringType(
        reinterpret_cast<const char_type*>(score + order_[i].offset),
        order_[i].bucket->size()
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
    size_t size_;
    IRESEARCH_API_PRIVATE_VARIABLES_END
  }; // prepared

  static const order& unordered();

  order() = default;
  order(order&& rhs) NOEXCEPT;

  order& operator=(order&& rhs) NOEXCEPT;

  bool operator==(const order& other) const;

  bool operator!=(const order& other) const;

  prepared prepare() const;

  order& add(bool reverse, sort::ptr const& sort);

  template<typename T, typename... Args>
  T& add(bool reverse, Args&&... args) {
    typedef typename std::enable_if <
      std::is_base_of< sort, T >::value, T
    >::type type;

    add(reverse, type::make(std::forward<Args>(args)...));
    return static_cast<type&>(order_.back().sort());
  }

  template<typename T>
  void remove() {
    typedef typename std::enable_if <
      std::is_base_of< sort, T >::value, T
    >::type type;

    remove(type::type());
  }

  void remove(const type_id& id);
  void clear() { order_.clear(); }

  size_t size() const NOEXCEPT { return order_.size(); }
  bool empty() const NOEXCEPT { return order_.empty(); }

  const_iterator begin() const { return order_.begin(); }
  const_iterator end() const { return order_.end(); }

  iterator begin() { return order_.begin(); }
  iterator end() { return order_.end(); }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  order_t order_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // order

NS_END

#endif
