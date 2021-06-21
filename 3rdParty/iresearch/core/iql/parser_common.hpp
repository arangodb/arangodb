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

#ifndef IRESEARCH_IQL_PARSER_COMMON_H
#define IRESEARCH_IQL_PARSER_COMMON_H

#include <functional>
#include <locale>
#include <vector>
#include <unordered_map>

#include "utils/string.hpp"

namespace iresearch {
  class order;

  namespace iql {
    class function_arg;
    class parser_context;
    class proxy_filter;

    template<typename deterministic_buffer_type, typename contextual_buffer_type, typename... contextual_ctx_args_type>
    class function {
    public:
      typedef contextual_buffer_type contextual_buffer_t;
      typedef function_arg contextual_function_arg_t;
      typedef std::vector<contextual_function_arg_t> contextual_function_args_t;
      typedef std::function<bool(contextual_buffer_t&, const contextual_ctx_args_type&..., const contextual_function_args_t&)> contextual_function_t;

      typedef deterministic_buffer_type deterministic_buffer_t;
      typedef iresearch::string_ref deterministic_function_arg_t;
      typedef std::vector<deterministic_function_arg_t> deterministic_function_args_t;
      typedef std::function<bool(deterministic_buffer_t&, const deterministic_function_args_t&)> deterministic_function_t;

      function(const deterministic_function_t& fnDeterminitic, const contextual_function_t& fnContextual, size_t nFixedArg = 0, bool bVarArg = false):
        m_fnContextual(fnContextual), m_fnDeterminitic(fnDeterminitic), m_nFixedArg(nFixedArg), m_bVarArg(bVarArg) {}
      function(const deterministic_function_t& fnDeterminitic, size_t nFixedArg = 0, bool bVarArg = false):
        function(fnDeterminitic, NOT_IMPLEMENTED_C, nFixedArg, bVarArg) {}
      function(const contextual_function_t& fnContextual, size_t nFixedArg = 0, bool bVarArg = false):
        function(NOT_IMPLEMENTED_D, fnContextual, nFixedArg, bVarArg) {}
      function& operator=(function&) = delete; // because of references
      bool operator==(const function& other) const {
        return
          &m_fnContextual == &(other.m_fnContextual) &&
          &m_fnDeterminitic == &(other.m_fnDeterminitic) &&
          m_nFixedArg == other.m_nFixedArg &&
          m_bVarArg == other.m_bVarArg;
      }

    private:
      static const contextual_function_t NOT_IMPLEMENTED_C;
      static const deterministic_function_t NOT_IMPLEMENTED_D;
      friend class parser_context;
      const contextual_function_t& m_fnContextual;
      const deterministic_function_t& m_fnDeterminitic;
      size_t m_nFixedArg;
      bool m_bVarArg;
    };

    class IRESEARCH_API function_arg {
     public:
      typedef std::vector<function_arg> fn_args_t;
      typedef std::function<bool(
        proxy_filter& node,
        const std::locale& locale,
        void* const& cookie,
        const fn_args_t& args
      )> fn_branch_t;
      typedef std::function<bool(
        bstring& buf,
        const std::locale& locale,
        void* const& cookie,
        const fn_args_t& args
      )> fn_value_t;

      function_arg(fn_args_t&& fnArgs, const fn_value_t& fnValue, const fn_branch_t& fnBranch = NOT_IMPLEMENTED_BRANCH);
      function_arg(fn_args_t&& fnArgs, const fn_branch_t& fnBranch);
      function_arg(fn_args_t&& fnArgs, const fn_value_t& fnValue, fn_branch_t&& fnBranch);
      function_arg(fn_args_t&& fnArgs, const fn_value_t&& fnValue, fn_branch_t& fnBranch);
      function_arg(fn_args_t&& fnArgs, fn_value_t&& fnValue, fn_branch_t&& fnBranch);
      function_arg(fn_args_t&& fnArgs, fn_branch_t&& fnBranch);
      function_arg(fn_args_t&& fnArgs, const bytes_ref& value, const fn_branch_t& fnBranch);
      function_arg(fn_args_t&& fnArgs, const bytes_ref& value, fn_branch_t&& fnBranch);
      function_arg(const bytes_ref& value);
      function_arg(function_arg&& other) noexcept;
      function_arg(const function_arg& other) = delete; // to avoid having multipe copies of args
      function_arg();
      function_arg& operator=(function_arg&& other) noexcept;
      function_arg& operator=(const function_arg& other) = delete; // to avoid having multipe copies of args
      bool branch(proxy_filter& buf, const std::locale& locale, void* const& cookie) const;
      bool value(bstring& buf, bool& bNil, const std::locale& locale, void* const& cookie) const;
      static function_arg wrap(const function_arg& wrapped);

    private:
      IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
      static const fn_branch_t NOT_IMPLEMENTED_BRANCH;
      fn_args_t m_fnArgs;
      bool m_bFnBranchRef;
      bool m_bFnValueRef;
      bool m_bValueNil;
      const fn_branch_t* m_pFnBranch;
      const fn_value_t* m_pFnValue;
      fn_branch_t m_fnBranch;
      fn_value_t m_fnValue;
      IRESEARCH_API_PRIVATE_VARIABLES_END
    };

    template<typename deterministic_buffer_type, typename contextual_buffer_type, typename... contextual_ctx_args_type>
    /*static*/ const typename function<deterministic_buffer_type, contextual_buffer_type, contextual_ctx_args_type...>::contextual_function_t
    function<deterministic_buffer_type, contextual_buffer_type, contextual_ctx_args_type...>::NOT_IMPLEMENTED_C =
      [](contextual_buffer_t&, const contextual_ctx_args_type&..., const contextual_function_args_t&)->bool { return false; };

    template<typename deterministic_buffer_type, typename contextual_buffer_type, typename... contextual_ctx_args_type>
    /*static*/ const typename function<deterministic_buffer_type, contextual_buffer_type, contextual_ctx_args_type...>::deterministic_function_t
    function<deterministic_buffer_type, contextual_buffer_type, contextual_ctx_args_type...>::NOT_IMPLEMENTED_D =
      [](deterministic_buffer_t&, const deterministic_function_args_t&)->bool { return false; };

    // deterministic(bool&, vector<string_ref>) v.s. contextual(proxy_filter&, std::locale, void* cookie, vector<bytes_ref>)
    typedef function<bool, iresearch::iql::proxy_filter, std::locale, void*> boolean_function;

    // deterministic(std::string&, vector<string_ref>) v.s. contextual(order&, std::locale, void* cookie, bool, vector<bytes_ref>)
    typedef function<std::string, iresearch::order, std::locale, void*, bool> order_function;

    // deterministic(std::string&, vector<string_ref>) v.s. contextual(bstring&, std::locale, void* cookie, vector<bytes_ref>)
    typedef function<std::string, iresearch::bstring, std::locale, void*> sequence_function;

    typedef std::unordered_multimap<std::string, boolean_function> boolean_functions;
    typedef std::unordered_multimap<std::string, order_function> order_functions;
    typedef std::unordered_multimap<std::string, sequence_function> sequence_functions;

    struct IRESEARCH_API functions {
      const boolean_functions& boolFns;
      const order_functions& orderFns;
      const sequence_functions& seqFns;
      explicit functions(
        const boolean_functions& bool_fns = DEFAULT().boolFns,
        const sequence_functions& seq_fns = DEFAULT().seqFns,
        const order_functions& order_fns = DEFAULT().orderFns
      ): boolFns(bool_fns), orderFns(order_fns), seqFns(seq_fns) {}
      explicit functions(
        const boolean_functions& bool_fns,
        const order_functions& order_fns
      ): functions(bool_fns, DEFAULT().seqFns, order_fns) {}
      explicit functions(
        const sequence_functions& seq_fns,
        const order_functions& order_fns = DEFAULT().orderFns
      ): functions(DEFAULT().boolFns, seq_fns, order_fns) {}
      explicit functions(const order_functions& order_fns):
        functions(DEFAULT().seqFns, order_fns) {}
      functions& operator=(functions&) = delete; // because of references
      static const functions& DEFAULT();
    };
  }
}

#endif
