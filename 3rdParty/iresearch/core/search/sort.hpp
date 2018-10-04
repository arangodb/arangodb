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

  boost();

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief object used for collecting index statistics for a specific term
  ///        that are required by the scorer for scoring individual documents
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API collector {
   public:
    DECLARE_UNIQUE_PTR(collector);
    DEFINE_FACTORY_INLINE(collector);

    virtual ~collector();

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief collect term level statistics, e.g. from current attribute values
    ///        called for the same term once for every segment
    ////////////////////////////////////////////////////////////////////////////////
    virtual void collect(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_view& term_attrs
    ) = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief store collected index statistics into attributes for the current
    ///        filter node
    ////////////////////////////////////////////////////////////////////////////////
    virtual void finish(
      attribute_store& filter_attrs,
      const index_reader& index
    ) = 0;
  }; // collector

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief stateful object used for computing the document score based on the
  ///        stored state
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API scorer {
   public:
    DECLARE_UNIQUE_PTR(scorer);
    DEFINE_FACTORY_INLINE(scorer);

    virtual ~scorer();

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

  ////////////////////////////////////////////////////////////////////////////////
  /// @class sort::prepared
  /// @brief base class for all prepared(compiled) sort entries
  ////////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API prepared : public util::attribute_view_provider {
   public:
    DECLARE_UNIQUE_PTR(prepared);

    prepared() = default;
    explicit prepared(attribute_view&& attrs);
    virtual ~prepared();

    using util::attribute_view_provider::attributes;
    virtual attribute_view& attributes() NOEXCEPT override final {
      return attrs_;
    }

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief the features required for proper operation of this sort::prepared
    ////////////////////////////////////////////////////////////////////////////////
    virtual const flags& features() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create an object to be used for collecting index statistics
    ////////////////////////////////////////////////////////////////////////////////
    virtual collector::ptr prepare_collector() const = 0;

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief create a stateful scorer used for computation of document scores
    ////////////////////////////////////////////////////////////////////////////////
    virtual scorer::ptr prepare_scorer(
      const sub_reader& segment,
      const term_reader& field,
      const attribute_store& query_attrs,
      const attribute_view& doc_attrs
    ) const = 0;

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

  explicit sort(const type_id& id);
  virtual ~sort();

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
  class entry {
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

    class IRESEARCH_API stats final : private util::noncopyable {
     public:
      typedef std::vector<sort::collector::ptr> collectors_t;

      explicit stats(collectors_t&& colls);
      stats(stats&& rhs) NOEXCEPT;

      stats& operator=(stats&& rhs) NOEXCEPT;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief collect term level statistics, e.g. from current attribute values
      ///        called for the same term once for every segment
      ////////////////////////////////////////////////////////////////////////////////
      void collect(
        const sub_reader& segment,
        const term_reader& field,
        const attribute_view& term_attrs
      ) const;

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief store collected index statistics into attributes for the current
      ///        filter node
      ////////////////////////////////////////////////////////////////////////////////
      void finish(
        attribute_store& filter_attrs,
        const index_reader& index
      ) const;

     private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      collectors_t colls_;
      IRESEARCH_API_PRIVATE_VARIABLES_END
    }; // collectors

    class IRESEARCH_API scorers final : private util::noncopyable {
     public:
      typedef std::vector<sort::scorer::ptr> scorers_t;

      scorers() = default;
      explicit scorers(scorers_t&& scorers);
      scorers(scorers&& rhs) NOEXCEPT;

      scorers& operator=(scorers&& rhs) NOEXCEPT;

      void score(const prepared& ord, byte_type* score) const;

     private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      scorers_t scorers_;
      IRESEARCH_API_PRIVATE_VARIABLES_END
    }; // scorers

    typedef std::vector<prepared_sort> prepared_order_t;

    static const prepared& unordered();

    prepared();
    prepared(prepared&& rhs) NOEXCEPT;

    prepared& operator=(prepared&& rhs) NOEXCEPT;

    const flags& features() const { return features_; }
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

    prepared::stats prepare_stats() const;

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
