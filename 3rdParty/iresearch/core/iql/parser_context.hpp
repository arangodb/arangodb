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

#ifndef IRESEARCH_IQL_PARSER_CONTEXT_H
#define IRESEARCH_IQL_PARSER_CONTEXT_H

#include <deque>
#include <unordered_map>
#include <vector>
#include "parser_common.hpp"

MSVC_ONLY(__pragma(warning(push)))
MSVC_ONLY(__pragma(warning(disable: 4146))) // unary minus operator applied to unsigned type, result still unsigned

  #include "iql/parser.hh"

MSVC_ONLY(__pragma(warning(pop)))

namespace iresearch {
  namespace iql {
    class parser_context: public context {
    public:
      parser_context(std::string const& sData, functions const& functions = functions::DEFAULT());
      parser_context& operator=(parser_context&) = delete; // because of references

      // parser operations
      virtual void yyerror(parser::location_type const& location, std::string const& sError) final override;
      virtual parser::token_type yylex(parser::semantic_type& value, parser::location_type& location) final override;

      // value operations
      virtual parser::semantic_type sequence(parser::location_type const& location) final override;

      // node operations
      virtual parser::semantic_type append(parser::semantic_type const& value, parser::location_type const& location) final override;
      virtual parser::semantic_type boost(parser::semantic_type const& value, parser::location_type const& location) final override;
      virtual parser::semantic_type function(parser::semantic_type const& name, parser::semantic_type const& args) final override;
      virtual parser::semantic_type list(parser::semantic_type const& value1, parser::semantic_type const& value2) final override;
      virtual parser::semantic_type negation(parser::semantic_type const& value) final override;
      virtual parser::semantic_type range(parser::semantic_type const& value1, bool bInclusive1, parser::semantic_type const& value2, bool bInclusive2) final override;

      // comparison operations
      virtual parser::semantic_type op_eq(parser::semantic_type const& value1, parser::semantic_type const& value2) final override;
      virtual parser::semantic_type op_like(parser::semantic_type const& value1, parser::semantic_type const& value2) final override;

      // filter operations
      virtual parser::semantic_type op_and(parser::semantic_type const& value1, parser::semantic_type const& value2) final override;
      virtual parser::semantic_type op_or(parser::semantic_type const& value1, parser::semantic_type const& value2) final override;

      // query operations
      virtual bool addOrder(parser::semantic_type const& value, bool bAscending = true) final override;
      virtual bool setLimit(parser::semantic_type const& value) final override;
      virtual bool setQuery(parser::semantic_type const& value) final override;
    protected:
      struct query_node {
        enum NodeType { UNKNOWN, UNION, INTERSECTION, FUNCTION, EQUAL, LIKE, LIST, RANGE, SEQUENCE, BOOL_TRUE } type = NodeType::UNKNOWN;

        // valid for FUNCTION
        boolean_function const* pFnBoolean;

        // valid for FUNCTION
        order_function const* pFnOrder;

        // valid for FUNCTION
        sequence_function const* pFnSequence;

        // valid for SEQUENCE, (for: FUNCTION - may contain function name, used by print(...) only)
        std::string sValue;

        // valid for RANGE
        bool bBeginInclusive;
        bool bEndInclusive;

        // valid for INTERSECTION, UNION, FUNCTION(pFnBoolean), EQUAL, LIKE
        float_t fBoost = 1.;

        // valid for FUNCTION(pFnBoolean), EQUAL, LIKE, TRUE
        bool bNegated = false;

        // valid for UNION, INTERSECTION, FUNCTION, EQUAL (size == 2), LIKE (size == 2), RANGE (size == 2), LIST
        std::vector<size_t> children;
      };
      struct query_position {
        std::string sMessage;
        size_t nStart;
        size_t nEnd;
      };
      struct query_state {
        size_t nOffset; // next offset position to be parsed
        size_t const* pnFilter; // the filter portion (nodeID) of the query, or nullptr if unset
        std::vector<std::pair<size_t, bool>> const& order; // the order portion (nodeID, ascending) of the query
        size_t const* pnLimit; // the limit value of the query, or nullptr if unset
        query_position const* pError; // the last encountered error, or nullptr if no errors seen
        query_state(
          size_t v_nOffset,
          size_t const* v_pnFilter,
          std::vector<std::pair<size_t, bool>> const& v_order,
          size_t const* v_pnLimit,
          query_position const* v_pError
        ): nOffset(v_nOffset), pnFilter(v_pnFilter), order(v_order), pnLimit(v_pnLimit), pError(v_pError) {}
        query_state& operator=(query_state&) = delete; // because of references
      };

      query_state current_state() const;
      query_node const& find_node(parser::semantic_type const& value) const;
      template<typename T>
      typename T::contextual_function_t const& function(T const& fn) const { return fn.m_fnContextual; }
      void print(std::ostream& out, parser::semantic_type const& root, bool bBoost = false, bool bId = false);
    private:
      std::string const& m_sData;
      std::pair<bool, query_position> m_error;
      std::pair<bool, size_t> m_filter;
      functions const& m_functions;
      std::pair<bool, size_t> m_limit;
      size_t m_nNext;
      std::deque<query_node> m_nodes; // a type that allows O(1) random access and does not invalidate on resize // FIXME seperate into different type vectors?
      std::unordered_map<size_t, size_t> m_negatedNodeCache;
      std::vector<std::pair<size_t, bool>> m_order;
      enum StateType { NONE, SINGLE, DOUBLE } m_eState;

      void add_child(std::vector<size_t>& children, parser::semantic_type const& child, bool bRemoveSuperset);
      query_node& create_node(parser::semantic_type& value);
      template <typename function_type>
      function_type const* find_best_function(
        std::string const& sName,
        size_t nArgsCount,
        std::unordered_multimap<std::string, function_type> const& fns
      ) const;
      query_node& find_node(parser::semantic_type const& value);
      parser::token_type next();
      parser::token_type nextKeyword();
      parser::token_type nextNumber();
      parser::token_type nextOperator();
      parser::token_type nextQuoted(bool bStart);
      parser::token_type nextSeperator();
      parser::token_type nextSequence(char cSep);
      parser::semantic_type try_eval(parser::semantic_type value, bool bBoolean, bool bOrder, bool bSequence);
    };
  }
}

#endif