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

#include <sstream>

#include "parser_context.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_stream.hpp"
#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "utils/locale_utils.hpp"
#include "query_builder.hpp"

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                   private members
// -----------------------------------------------------------------------------

// by default no transformation is performed and value is treated verbatim
const irs::iql::query_builder::branch_builder_function_t RANGE_EE_BRANCH_BUILDER =
  [](irs::iql::proxy_filter& root,
     const std::locale& locale,
     const irs::string_ref& field,
     void* cookie,
     const std::vector<irs::iql::function_arg>& args)->bool {
    irs::bstring minValue;
    irs::bstring maxValue;
    bool bMinValueNil;
    bool bMaxValueNil;

    if (args.size() != 2 ||
        !args[0].value(minValue, bMinValueNil, locale, cookie) ||
        !args[1].value(maxValue, bMaxValueNil, locale, cookie)) {
      return false;
    }

    auto& range = root.proxy<irs::by_range>();
    range.mutable_field()->assign(field.c_str(), field.size());

    auto& search_range = range.mutable_options()->range;

    if (!bMinValueNil) {
      search_range.min = std::move(minValue);
      search_range.min_type = irs::BoundType::EXCLUSIVE;
    }

    if (!bMaxValueNil) {
      search_range.max = std::move(maxValue);
      search_range.max_type = irs::BoundType::EXCLUSIVE;
    }

    return true;
  };

const irs::iql::query_builder::branch_builder_function_t RANGE_EI_BRANCH_BUILDER =
  [](irs::iql::proxy_filter& root,
     const std::locale& locale,
     const irs::string_ref& field,
     void* cookie,
     const std::vector<irs::iql::function_arg>& args)->bool {
     irs::bstring minValue;
     irs::bstring maxValue;
    bool bMinValueNil;
    bool bMaxValueNil;

    if (args.size() != 2 ||
        !args[0].value(minValue, bMinValueNil, locale, cookie) ||
        !args[1].value(maxValue, bMaxValueNil, locale, cookie)) {
      return false;
    }

    auto& range = root.proxy<irs::by_range>();
    range.mutable_field()->assign(field.c_str(), field.size());

    auto& search_range = range.mutable_options()->range;

    if (!bMinValueNil) {
      search_range.min = std::move(minValue);
      search_range.min_type = irs::BoundType::EXCLUSIVE;
    }

    if (!bMaxValueNil) {
      search_range.max = std::move(maxValue);
      search_range.max_type = irs::BoundType::INCLUSIVE;
    }

    return true;
  };

const irs::iql::query_builder::branch_builder_function_t RANGE_IE_BRANCH_BUILDER =
  [](irs::iql::proxy_filter& root,
     const std::locale& locale,
     const irs::string_ref& field,
     void* cookie,
     const std::vector<irs::iql::function_arg>& args)->bool {
    irs::bstring minValue;
    irs::bstring maxValue;
    bool bMinValueNil;
    bool bMaxValueNil;

    if (args.size() != 2 ||
        !args[0].value(minValue, bMinValueNil, locale, cookie) ||
        !args[1].value(maxValue, bMaxValueNil, locale, cookie)) {
      return false;
    }

    auto& range = root.proxy<irs::by_range>();
    range.mutable_field()->assign(field.c_str(), field.size());

    auto& search_range = range.mutable_options()->range;

    if (!bMinValueNil) {
      search_range.min = std::move(minValue);
      search_range.min_type = irs::BoundType::INCLUSIVE;
    }

    if (!bMaxValueNil) {
      search_range.max = std::move(maxValue);
      search_range.max_type = irs::BoundType::EXCLUSIVE;
    }

    return true;
  };

const irs::iql::query_builder::branch_builder_function_t RANGE_II_BRANCH_BUILDER =
  [](irs::iql::proxy_filter& root,
     const std::locale& locale,
     const irs::string_ref& field,
     void* cookie,
     const std::vector<irs::iql::function_arg>& args)->bool {
    irs::bstring minValue;
    irs::bstring maxValue;
    bool bMinValueNil;
    bool bMaxValueNil;

    if (args.size() != 2 ||
        !args[0].value(minValue, bMinValueNil, locale, cookie) ||
        !args[1].value(maxValue, bMaxValueNil, locale, cookie)) {
      return false;
    }

    if (bMinValueNil && bMaxValueNil) {
      // exact equivalence optimization for nil value
      auto& filter = root.proxy<irs::by_term>();
      *filter.mutable_field() = field;
      filter.mutable_options()->term.clear();
    }
    else if (!bMinValueNil && !bMaxValueNil && minValue == maxValue) {
      // exact equivalence optimization
      auto& filter = root.proxy<irs::by_term>();
      *filter.mutable_field() = field;
      filter.mutable_options()->term = std::move(minValue);
    }
    else {
      auto& range = root.proxy<irs::by_range>();
      range.mutable_field()->assign(field.c_str(), field.size());

      auto& search_range = range.mutable_options()->range;

      if (!bMinValueNil) {
        search_range.min = std::move(minValue);
        search_range.min_type = irs::BoundType::INCLUSIVE;
      }

      if (!bMaxValueNil) {
        search_range.max = std::move(maxValue);
        search_range.max_type = irs::BoundType::INCLUSIVE;
      }
    }

    return true;
  };

const irs::iql::query_builder::branch_builder_function_t SIMILAR_BRANCH_BUILDER =
  [](
    irs::iql::proxy_filter& root,
    const std::locale& locale,
    const irs::string_ref& field,
    void* cookie,
    const std::vector<irs::iql::function_arg>& args
  )->bool {
    irs::bstring value;
    bool bValueNil;

    if (args.size() != 1 ||
        !args[0].value(value, bValueNil, locale, cookie)) {
      return false;
    }

    const irs::string_ref value_ref(bValueNil ? irs::string_ref::NIL : irs::ref_cast<char>(value));
    auto tokens = irs::analysis::analyzers::get(
      "text", irs::type<irs::text_format::text>::get(), irs::locale_utils::name(locale));

    if (!tokens || !tokens->reset(value_ref)) {
      return false;
    }

    auto& node = root.proxy<irs::by_phrase>();
    *node.mutable_field() = field;

    for (auto* term = irs::get<irs::term_attribute>(*tokens); tokens->next();) {
      auto& part = node.mutable_options()->push_back(irs::by_term_options{});
      part.term = term->value;
    }

    return true;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

  class parse_context;

  class ErrorNode: public irs::filter {
   public:
    ErrorNode(): filter(irs::type<ErrorNode>::get()) {}
    irs::filter::prepared::ptr prepare(
        const irs::index_reader&,
        const irs::order::prepared&,
        irs::boost_t,
        const irs::attribute_provider*) const override {
      irs::filter::prepared::ptr result; // null-ptr result
      return result;
    }

   private:
    friend class parse_context;
    std::string sError;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief proxy_filter specialized for irs::filter::ptr
  ////////////////////////////////////////////////////////////////////////////////
  class LinkNode: public irs::iql::proxy_filter_t<std::shared_ptr<irs::filter>> {
   public:
    LinkNode(irs::filter* link): proxy_filter_t(irs::type<LinkNode>::get()) {
      filter_ = ptr(link);
    }

    LinkNode(const LinkNode& other): proxy_filter_t(irs::type<LinkNode>::get()) {
      filter_ = other.filter_;
    }

    template<typename... Args>
    static ptr make(Args&&... args) {
      return irs::memory::make_unique<LinkNode>(std::forward<Args>(args)...);
    }
  };

  class RootNode: public irs::Or {
   public:
    DECLARE_FACTORY();

   private:
    friend class parse_context;
    irs::order order;
    size_t nLimit;
  };
  DEFINE_FACTORY_DEFAULT(RootNode)

  class parse_context: public irs::iql::parser_context {
   public:
    parse_context(
      const std::string& sQuery,
      const std::locale& locale,
      void* cookie,
      const irs::iql::functions& functions,
      const irs::iql::query_builder::branch_builders& branch_builders
    );
    irs::iql::query build();
    irs::iql::query buildError();

   private:
    static const irs::iql::parser::semantic_type SUCCESS;
    static const irs::iql::parser::semantic_type UNKNOWN;
    const irs::iql::query_builder::branch_builders& m_branch_builders;
    void* m_cookie;
    const std::locale& m_locale;

    irs::iql::parser::semantic_type append_function_arg(
      irs::iql::function_arg::fn_args_t& buf,
      const irs::iql::parser::semantic_type& node_id
    ) const; // @return SUCCESS or ID of failed node, or UNKNOWN for self
    irs::iql::parser::semantic_type children(
      irs::boolean_filter& node, const std::vector<size_t>& children
    ) const; // @return SUCCESS or ID of failed node, or UNKNOWN for self
    irs::iql::parser::semantic_type eval(
      irs::bstring& buf,
      const query_node& src
    ) const; // @return SUCCESS or ID of failed node, or UNKNOWN for self
    template <typename fn_type, typename... ctx_args_type>
    irs::iql::parser::semantic_type eval(
      typename fn_type::contextual_buffer_t& buf,
      const typename fn_type::contextual_function_t& fn,
      const std::vector<size_t>& args,
      const ctx_args_type&... ctx_args
    ) const; // @return SUCCESS or ID of failed node, or UNKNOWN for self
    query_node const& find_node(
      irs::iql::parser::semantic_type const& value
    ) const {
      // force visibility only of const fn for GCC, otherwise it tries private fn
      return parser_context::find_node(value);
    }
    template <typename T> // @return SUCCESS or ID of failed node, or UNKNOWN for self
    irs::iql::parser::semantic_type init(T& node, const query_node& src) const;
    irs::iql::parser::semantic_type initRange(
      irs::iql::proxy_filter& node,
      const irs::string_ref& field,
      const irs::iql::parser::semantic_type& min_node_id, bool min_inclusive,
      const irs::iql::parser::semantic_type& max_value_id, bool max_inclusive
    ) const;
    irs::iql::parser::semantic_type initSimilar(
      irs::iql::proxy_filter& node,
      const irs::string_ref& field,
      const irs::iql::parser::semantic_type& value_id
    ) const;
    irs::iql::parser::semantic_type order(
      irs::order& node, const std::vector<std::pair<size_t, bool>>& order
    ) const; // @return SUCCESS or ID of failed node
  };

  const irs::iql::parser::semantic_type parse_context::SUCCESS =
    std::numeric_limits<irs::iql::parser::semantic_type>::max(); // init() success
  const irs::iql::parser::semantic_type parse_context::UNKNOWN =
    std::numeric_limits<irs::iql::parser::semantic_type>::max() - 1; // init() unknown failure

  parse_context::parse_context(
    const std::string& sQuery,
    const std::locale& locale,
    void* cookie,
    const irs::iql::functions& functions,
    const irs::iql::query_builder::branch_builders& branch_builders
  ):
    parser_context(sQuery, functions),
    m_branch_builders(branch_builders),
    m_cookie(cookie),
    m_locale(locale) {
  }

  // add implementation before any calls to the function
  template<typename fn_type, typename... ctx_args_type>
  irs::iql::parser::semantic_type parse_context::eval(
    typename fn_type::contextual_buffer_t& buf,
    const typename fn_type::contextual_function_t& fn,
    const std::vector<size_t>& args,
    const ctx_args_type&... ctx_args
  ) const {
    irs::iql::function_arg::fn_args_t fnArgs;

    fnArgs.reserve(args.size());

    // build function argument list
    for (size_t i = 0, count = args.size(); i < count; ++i) {
      auto errorNodeId = append_function_arg(fnArgs, args[i]);

      if (SUCCESS != errorNodeId) {
        return UNKNOWN == errorNodeId ? args[i] : errorNodeId;
      }
    }

    return fn(buf, ctx_args..., fnArgs) ? SUCCESS : UNKNOWN;
  }

  template<> // add implementation before any calls to the function
  irs::iql::parser::semantic_type parse_context::init<irs::iql::proxy_filter>(
    irs::iql::proxy_filter& node, const query_node& src
  ) const {
    assert(
      (query_node::NodeType::FUNCTION == src.type && src.pFnBoolean) ||
      query_node::NodeType::EQUAL == src.type ||
      query_node::NodeType::LIKE  == src.type
    );

    node.boost(src.fBoost);

    if (query_node::NodeType::FUNCTION == src.type && src.pFnBoolean) {
      return eval<irs::iql::boolean_function, std::locale, void*>(
        node, function(*(src.pFnBoolean)), src.children, m_locale, m_cookie
      );
    }

    assert(src.children.size() == 2); // 2 - left and right side of operator

    auto& left = find_node(src.children[0]);
    irs::bstring fieldBuf;
    {
      auto errorNodeId = eval(fieldBuf, left);

      if (SUCCESS != errorNodeId) {
        return UNKNOWN == errorNodeId ? src.children[0] : errorNodeId;
      }
    }

    auto field = irs::ref_cast<char>(fieldBuf);

    switch(src.type) {
     case query_node::NodeType::EQUAL: {
      // iresearch:iql::parser implementation returns range on right
      auto& right = find_node(src.children[1]);
      assert(query_node::NodeType::RANGE == right.type);
      assert(right.children.size() == 2); // 2 - min and max value of range

      auto errorNodeId = initRange(
        node, field,
        right.children[0], right.bBeginInclusive,
        right.children[1], right.bEndInclusive
      );

      return UNKNOWN == errorNodeId ? src.children[1] : errorNodeId;
     }
     case query_node::NodeType::LIKE: {
      auto errorNodeId = initSimilar(node, field, src.children[1]);

      return UNKNOWN == errorNodeId ? src.children[1] : errorNodeId;
     }
     default: {} // NOOP
    }

    return UNKNOWN;
  }

  template<> // add implementation before any calls to the function
  irs::iql::parser::semantic_type parse_context::init<irs::Or>(
    irs::Or& node, const query_node& src
  ) const {
    assert(query_node::NodeType::UNION == src.type);
    node.boost(src.fBoost);

    return children(node, src.children);
  }

  template<> // add implementation before any calls to the function
  irs::iql::parser::semantic_type parse_context::init<irs::And>(
    irs::And& node, const query_node& src
  ) const {
    assert(query_node::NodeType::INTERSECTION == src.type);
    node.boost(src.fBoost);

    return children(node, src.children);
  }

  template<> // add implementation before any calls to the function
  irs::iql::parser::semantic_type parse_context::init<irs::Not>(
    irs::Not& node, const query_node& src
  ) const {
    assert(
      (query_node::NodeType::FUNCTION == src.type && src.pFnBoolean) ||
      query_node::NodeType::EQUAL == src.type ||
      query_node::NodeType::LIKE == src.type
    );

    return init(node.filter<irs::iql::proxy_filter>(), src);
  }

  template<> // add implementation before any calls to the function
  irs::iql::parser::semantic_type parse_context::init<irs::all>(
    irs::all& node, const query_node& src
  ) const {
    assert(query_node::NodeType::BOOL_TRUE == src.type);
    node.boost(src.fBoost);

    return SUCCESS;
  }

  irs::iql::query parse_context::build() {
    auto state = current_state();
    irs::iql::query result;

    if (!state.pnFilter) {
      return buildError();
    }

    query_node root;

    root.type = query_node::NodeType::UNION;
    root.children.push_back(*state.pnFilter);
    result.filter = RootNode::make();

    auto errorNodeId =
      init(*static_cast<irs::Or*>(result.filter.get()), root);

    // initialize filter
    if (SUCCESS != errorNodeId) {
      std::stringstream error;

      error << "filter conversion error, node: @" << errorNodeId << std::endl;
      print(error, *state.pnFilter, false, true);
      result.filter = irs::memory::make_unique<ErrorNode>();
      result.error = &(static_cast<ErrorNode*>(result.filter.get())->sError);
      *(result.error) = error.str();

      return result;
    }

    // initialize order
    if (!state.order.empty()) {
      errorNodeId =
        order(static_cast<RootNode*>(result.filter.get())->order, state.order);

      if (SUCCESS != errorNodeId) {
        std::string sDelim = "";
        std::stringstream error;

        error << "order conversion error, node: @" << errorNodeId << std::endl;

        for (auto orderTerm: state.order) {
          error << sDelim;
          print(error, orderTerm.first, false, true);
          error << (orderTerm.second ? " ASC": " DESC");
          sDelim = ", ";
        }

        result.filter = irs::memory::make_unique<ErrorNode>();
        result.error = &(static_cast<ErrorNode*>(result.filter.get())->sError);
        *(result.error) = error.str();

        return result;
      }

      result.order = &(static_cast<RootNode*>(result.filter.get())->order);
    }

    // initialize limit
    if (state.pnLimit) {
      result.limit = &(static_cast<RootNode*>(result.filter.get())->nLimit);
      *(result.limit) = *(state.pnLimit);
    }

    return result;
  }

  irs::iql::query parse_context::buildError() {
    auto state = current_state();
    std::stringstream error;

    error << "@";

    if (!state.pError) {
      error << "(" << state.nOffset << "): parse error";
    }
    else {
      error << "("
        << "[" << state.pError->nStart
        << " - " << state.pError->nEnd << "], "
        << state.nOffset << "): "
        << state.pError->sMessage;
    }

    irs::iql::query result;

    result.filter = irs::memory::make_unique<ErrorNode>();
    result.error = &(static_cast<ErrorNode*>(result.filter.get())->sError);
    *(result.error) = error.str();

    return result;
  }

  irs::iql::parser::semantic_type parse_context::append_function_arg(
    irs::iql::function_arg::fn_args_t& buf,
    const irs::iql::parser::semantic_type& node_id
  ) const {
    irs::iql::function_arg::fn_args_t fnArgs;
    auto& node = find_node(node_id);

    switch(node.type) {
     case query_node::NodeType::UNION:
      buf.emplace_back(
        std::move(fnArgs),
        [this, node_id](
          irs::iql::proxy_filter& node, const std::locale&, void* const&, const irs::iql::function_arg::fn_args_t& args
        )->bool {
          return args.empty() && SUCCESS == init(node.proxy<irs::Or>(), find_node(node_id));
        }
      );
      return SUCCESS;
     case query_node::NodeType::INTERSECTION:
      buf.emplace_back(std::move(fnArgs), [this, node_id](
        irs::iql::proxy_filter& node, const std::locale&, void* const&, const irs::iql::function_arg::fn_args_t& args
      )->bool {
        return args.empty() && SUCCESS == init(node.proxy<irs::And>(), find_node(node_id));
      });
      return SUCCESS;
     case query_node::NodeType::BOOL_TRUE:
      buf.emplace_back(std::move(fnArgs), [this, node_id](
        irs::iql::proxy_filter& node, const std::locale&, void* const&, const irs::iql::function_arg::fn_args_t& args
      )->bool {
        auto& argNode = find_node(node_id);
        return args.empty() && SUCCESS == init(
          argNode.bNegated
          ? node.proxy<irs::Not>().filter<irs::all>()
          : node.proxy<irs::all>(),
          argNode
        );
      });
      return SUCCESS;
     case query_node::NodeType::FUNCTION:
      fnArgs.reserve(node.children.size());

      // build function argument list
      for (size_t i = 0, count = node.children.size(); i < count; ++i) {
        auto& argNodeId = node.children[i];
        auto errorNodeId = append_function_arg(fnArgs, argNodeId);

        if (SUCCESS != errorNodeId) {
          return UNKNOWN == errorNodeId ? argNodeId : errorNodeId;
        }
      }

      if (node.pFnBoolean) {
        if (node.bNegated) {
          auto fnBranchArg = [this, node_id](
            irs::iql::proxy_filter& node,
            const std::locale& locale,
            void* const& cookie,
            const std::vector<irs::iql::function_arg>& args
          )->bool {
            auto& argNode = find_node(node_id);
            return argNode.pFnBoolean && function(*(argNode.pFnBoolean))(
              node.proxy<irs::Not>().filter<irs::iql::proxy_filter>(), locale, cookie, args
            );
          };

          if (node.pFnSequence) {
            buf.emplace_back(std::move(fnArgs), function(*(node.pFnSequence)), std::move(fnBranchArg));
          }
          else {
            buf.emplace_back(std::move(fnArgs), std::move(fnBranchArg));
          }

          return SUCCESS;
        }

        if (node.pFnSequence) {
          buf.emplace_back(std::move(fnArgs), function(*(node.pFnSequence)), function(*(node.pFnBoolean)));
        }
        else {
          buf.emplace_back(std::move(fnArgs), function(*(node.pFnBoolean)));
        }

        return SUCCESS;
      }

      if (node.pFnSequence) {
        buf.emplace_back(std::move(fnArgs), function(*(node.pFnSequence)));

        return SUCCESS;
      }

      return node_id; // not a supported function type
     case query_node::NodeType::EQUAL: // fall through
     case query_node::NodeType::LIKE:
      buf.emplace_back(std::move(fnArgs), [this, node_id](
        irs::iql::proxy_filter& node, const std::locale&, void* const&, const irs::iql::function_arg::fn_args_t& args
      )->bool {
        auto& argNode = find_node(node_id);
        return args.empty() && SUCCESS == init(
          argNode.bNegated
          ? node.proxy<irs::Not>().filter<irs::iql::proxy_filter>()
          : node,
          argNode
        );
      });
      return SUCCESS;
     case query_node::NodeType::SEQUENCE:
      buf.emplace_back(irs::ref_cast<irs::byte_type>(irs::string_ref(node.sValue)));
      return SUCCESS;
     default: {} // NOOP
    }

    return node_id; // unknown arg type
  }

  // initialize node children
  irs::iql::parser::semantic_type parse_context::children(
    irs::boolean_filter& node, const std::vector<size_t>& children
  ) const {
    for (auto& childId: children) {
      auto errorNodeId = childId;
      auto& child = find_node(childId);

      switch (child.type) {
       case query_node::NodeType::UNION:
        errorNodeId = init(node.add<irs::Or>(), child);
        break;
       case query_node::NodeType::INTERSECTION:
        errorNodeId = init(node.add<irs::And>(), child);
        break;
       case query_node::NodeType::FUNCTION:
        if (!child.pFnBoolean) {
          break; // only boolean functions supported as branches
        }
        // fall through
       case query_node::NodeType::EQUAL:
       case query_node::NodeType::LIKE:
        errorNodeId = child.bNegated
                    ? init(node.add<irs::Not>(), child)
                    : init(node.add<irs::iql::proxy_filter>(), child);
        break;
       default: {} // NOOP
      }

      // on error terminate traversal
      if (SUCCESS != errorNodeId) {
        return UNKNOWN == errorNodeId ? childId : errorNodeId;
      }
    }

    return SUCCESS;
  }

  irs::iql::parser::semantic_type parse_context::eval(
    irs::bstring& buf,
    const query_node& src
  ) const {
    switch(src.type) {
     case query_node::NodeType::SEQUENCE:
      buf.append(irs::ref_cast<irs::byte_type>(irs::string_ref(src.sValue)));
      break;
     case query_node::NodeType::FUNCTION:
      if (src.pFnSequence) {
        auto errorNodeId = eval<irs::iql::sequence_function, std::locale, void*>(
          buf, function(*(src.pFnSequence)), src.children, m_locale, m_cookie
        );

        // on error terminate traversal
        if (SUCCESS != errorNodeId) {
          return errorNodeId;
        }

        break;
      }
     [[fallthrough]];
     default:
      return UNKNOWN;
    }

    return SUCCESS;
  }

  irs::iql::parser::semantic_type parse_context::order(
    irs::order& node, const std::vector<std::pair<size_t, bool>>& order
  ) const {
    for (auto orderTerm: order) {
      auto& ascending = orderTerm.second;
      auto& childId = orderTerm.first;
      auto& childNode = find_node(childId);

      switch(childNode.type) {
       case query_node::NodeType::FUNCTION: {
        if (!childNode.pFnOrder) {
          return childId; // no order function
        }

        auto errorNodeId = eval<irs::iql::order_function, std::locale, void*, bool>(
          node,
          function(*(childNode.pFnOrder)),
          childNode.children,
          m_locale,
          m_cookie,
          ascending
        );

        // on error terminate traversal
        if (SUCCESS != errorNodeId) {
          return UNKNOWN == errorNodeId ? childId : errorNodeId;
        }

        break;
       }
       case query_node::NodeType::SEQUENCE:
        // field-term order is not yet supported by irs::order
       default:
        return childId;
      }
    }

    return SUCCESS;
  }

  irs::iql::parser::semantic_type parse_context::initRange(
    irs::iql::proxy_filter& node,
    const irs::string_ref& field,
    const irs::iql::parser::semantic_type& min_node_id, bool min_inclusive,
    const irs::iql::parser::semantic_type& max_value_id, bool max_inclusive
  ) const {
    auto& min_node = find_node(min_node_id);
    auto& max_node = find_node(max_value_id);
    irs::iql::function_arg::fn_args_t args;

    args.reserve(2);

    if (query_node::NodeType::UNKNOWN == min_node.type) {
      args.emplace_back(irs::bytes_ref::NIL);
    }
    else {
      auto errorNodeId = append_function_arg(args, min_node_id);

      if (SUCCESS != errorNodeId) {
        return UNKNOWN == errorNodeId ? min_node_id : errorNodeId;
      }
    }

    if (query_node::NodeType::UNKNOWN == max_node.type) {
      args.emplace_back(irs::bytes_ref::NIL);
    }
    else {
      auto errorNodeId = append_function_arg(args, max_value_id);

      if (SUCCESS != errorNodeId) {
        return UNKNOWN == errorNodeId ? min_node_id : errorNodeId;
      }
    }

    // .............................................................................
    // build a branch off of 'node' for IQL field/value
    // .............................................................................

    if (min_inclusive) {
      if (max_inclusive) {
        return m_branch_builders.range_ii(node, m_locale, field, m_cookie, args)
          ? SUCCESS : UNKNOWN;
      }

      return m_branch_builders.range_ie(node, m_locale, field, m_cookie, args)
        ? SUCCESS : UNKNOWN;
    }

    if (max_inclusive) {
      return m_branch_builders.range_ei(node, m_locale, field, m_cookie, args)
        ? SUCCESS : UNKNOWN;
    }

    return m_branch_builders.range_ee(node, m_locale, field, m_cookie, args)
      ? SUCCESS : UNKNOWN;
  }

  irs::iql::parser::semantic_type parse_context::initSimilar(
    irs::iql::proxy_filter& node,
    const irs::string_ref& field,
    const irs::iql::parser::semantic_type& value_id
  ) const {
    irs::iql::function_arg::fn_args_t args;

    args.reserve(1);

    auto errorNodeId = append_function_arg(args, value_id);

    if (SUCCESS != errorNodeId) {
      return UNKNOWN == errorNodeId ? value_id : errorNodeId;
    }

    // build a branch off of 'node' for IQL field/value
    return m_branch_builders.similar(node, m_locale, field, m_cookie, args)
      ? SUCCESS : UNKNOWN;
  }

}

namespace iresearch {
namespace iql {

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

query_builder::branch_builders::branch_builders(
  const branch_builder_function_t* v_range_ee /*= nullptr*/,
  const branch_builder_function_t* v_range_ei /*= nullptr*/,
  const branch_builder_function_t* v_range_ie /*= nullptr*/,
  const branch_builder_function_t* v_range_ii /*= nullptr*/,
  const branch_builder_function_t* v_similar /*= nullptr*/
): range_ee(v_range_ee ? *v_range_ee : RANGE_EE_BRANCH_BUILDER),
   range_ei(v_range_ei ? *v_range_ei : RANGE_EI_BRANCH_BUILDER),
   range_ie(v_range_ie ? *v_range_ie : RANGE_IE_BRANCH_BUILDER),
   range_ii(v_range_ii ? *v_range_ii : RANGE_II_BRANCH_BUILDER),
   similar(v_similar ? *v_similar : SIMILAR_BRANCH_BUILDER) {
}

query_builder::query_builder(
  const functions& iql_functions /*= functions::DEFAULT()*/,
  const branch_builders& branch_builders /*= branch_builders::DEFAULT()*/
): branch_builders_(branch_builders), iql_functions_(iql_functions) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

/*static*/ const query_builder::branch_builders& query_builder::branch_builders::DEFAULT() {
  static const branch_builders BRANCH_BUILDERS(
    nullptr, nullptr, nullptr, nullptr, nullptr
  );

  return BRANCH_BUILDERS;
}

query query_builder::build(
  const std::string& query,
  const std::locale& locale,
  void* cookie /*= nullptr*/,
  proxy_filter* root /*= nullptr*/
) const {
  parse_context ctx(query, locale, cookie, iql_functions_, branch_builders_);
  parser parser(ctx);

  if (parser.parse()) {
    return ctx.buildError();
  }

  if (!root) {
    return ctx.build();
  }

  auto result = ctx.build();

  // link the query into both the root and result.filter via two LinkNodes
  if (!result.error) {
    auto& link = root->proxy<LinkNode>(result.filter.get());
    result.filter.release();

    result.filter = LinkNode::make(link);
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

DEFINE_FACTORY_DEFAULT(proxy_filter)

} // iql
} // namespace iresearch {
