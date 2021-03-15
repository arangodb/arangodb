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

#ifndef IRESEARCH_IQL_QUERY_BUILDER_H
#define IRESEARCH_IQL_QUERY_BUILDER_H

#include "shared.hpp"
#include "parser_common.hpp"
#include "search/filter.hpp"

namespace iresearch {
  namespace iql {
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief class for passing control to the proxied query
    ////////////////////////////////////////////////////////////////////////////////
    template<typename PTR>
    class IRESEARCH_API proxy_filter_t: public irs::filter {
     public:
      proxy_filter_t(const type_info& type): irs::filter(type) {}
      template<typename T, typename... Args>
      T& proxy(Args&&... args) {
        typedef typename std::enable_if<
          std::is_base_of<irs::filter, T>::value, T
        >::type type;

        filter_ = type::make(std::forward<Args>(args)...);

        return static_cast<type&>(*filter_);
      }

      virtual size_t hash() const noexcept override {
        return !filter_ ? 0 : filter_->hash();
      }
      virtual irs::filter::prepared::ptr prepare(
          const index_reader& rdr,
          const order::prepared& ord,
          boost_t boost,
          const attribute_provider* ctx) const override {
        return filter_->prepare(rdr, ord, boost, ctx);
      };

     protected:
      virtual bool equals(const irs::filter& rhs) const noexcept override {
        const auto& typed_rhs =
          static_cast<const irs::iql::proxy_filter_t<PTR>&>(rhs);

        return filter::equals(rhs) && filter_.get() == typed_rhs.filter_.get();
      }

     protected:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      PTR filter_;
      IRESEARCH_API_PRIVATE_VARIABLES_END
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief proxy_filter specialized for irs::filter::ptr
    ////////////////////////////////////////////////////////////////////////////////
    class IRESEARCH_API proxy_filter: public proxy_filter_t<irs::filter::ptr> {
     public:
      DECLARE_FACTORY();

      proxy_filter(): proxy_filter_t(irs::type<proxy_filter>::get()) {}
    };

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief result of the build(...) operation
    ///        filter set to the root of the query
    ///        limit iff no limit requested in query then nullptr, else limit value
    ///        error iff no error then nullptr, else the error string
    ////////////////////////////////////////////////////////////////////////////////
    struct query {
      irs::filter::ptr filter;
      irs::order* order = nullptr;
      size_t* limit = nullptr;
      std::string* error = nullptr;
      query(): order(nullptr), limit(nullptr), error(nullptr) {}
      query(query&& other):
        filter(std::move(other.filter)),
        order(other.order),
        limit(other.limit),
        error(other.error) {
      }
    };
    static_assert(std::is_move_constructible<query>::value,
                  "default move constructor expected");
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief helper class for transforming a textual query into an iResearch query
    ////////////////////////////////////////////////////////////////////////////////
    class IRESEARCH_API query_builder {
     public:
       ////////////////////////////////////////////////////////////////////////////////
       /// @brief fns to build an iResearch query branch based at 'root' for field/args
       /// @param root the root of the future branch
       /// @param locale the locale to be used for building the branch (if needed)
       /// @param field the field name mentioned in the IQL query
       /// @param cookie the user supplied cookie, passed verbatim to callees
       /// @param args the function arguments
       /// @return the branch was constructed successfully
       ////////////////////////////////////////////////////////////////////////////////
       typedef std::function<bool(
         proxy_filter& root,
         const std::locale& locale,
         const string_ref& field,
         void* const& cookie,
         const function_arg::fn_args_t& args
       )> branch_builder_function_t;

       struct IRESEARCH_API branch_builders {
         const branch_builder_function_t& range_ee; // range exclude/exclude - (,)
         const branch_builder_function_t& range_ei; // range exclude/include - (,]
         const branch_builder_function_t& range_ie; // range include/exclude - [,)
         const branch_builder_function_t& range_ii; // range include/exclude - [,]
         const branch_builder_function_t& similar;
         branch_builders(
           const branch_builder_function_t* range_ee = nullptr, // nullptr == default
           const branch_builder_function_t* range_ei = nullptr, // nullptr == default
           const branch_builder_function_t* range_ie = nullptr, // nullptr == default
           const branch_builder_function_t* range_ii = nullptr, // nullptr == default
           const branch_builder_function_t* similar = nullptr // nullptr == default
         );
         branch_builders& operator=(branch_builders&) = delete; // because of refs
         static const branch_builders& DEFAULT();
       };

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief constructor (use defaults for nullptr param values)
      /// @brief iql_functions user defined functions callable within the IQL query
      /// @brief fv_transform transform of IQL field/value into iResearch field/value
      ////////////////////////////////////////////////////////////////////////////////
      query_builder(
        const functions& iql_functions = functions::DEFAULT(),
        const branch_builders& branch_builders = branch_builders::DEFAULT()
      );
      explicit query_builder(const branch_builders& branch_builders):
        query_builder(functions::DEFAULT(), branch_builders) {}

      query_builder& operator=(query_builder&) = delete; // because of references

      ////////////////////////////////////////////////////////////////////////////////
      /// @brief transform a textual query into an iResearch query
      /// @param query the query to parse/build
      /// @param locale the locale to use for parsing/building the query
      /// @param cookie a user-defined value passed verbatum to function invocations
      /// @param root the node to attach the query under (if not null)
      /// @return parsed query, iff error <return>.error != nullptr
      ////////////////////////////////////////////////////////////////////////////////
      query build(
        const std::string& query,
        const std::locale& locale,
        void* cookie = nullptr,
        proxy_filter* root = nullptr
      ) const;

     private:
      const branch_builders& branch_builders_;
      const functions& iql_functions_;
    };
  }
}

#endif
