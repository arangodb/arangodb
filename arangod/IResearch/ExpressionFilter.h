////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_FILTER
#define ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_FILTER 1

#include "Aql/Expression.h"

#include "search/filter.hpp"

NS_BEGIN(arangodb)

NS_BEGIN(aql)
struct AstNode;
class Ast;
class ExecutionPlan;
NS_END // aql

NS_BEGIN(iresearch)

class ByExpression final : public irs::filter {
 public:
  DECLARE_FACTORY_DEFAULT();
  DECLARE_FILTER_TYPE();

  ByExpression() noexcept;

  void init(
      aql::ExecutionPlan& plan,
      aql::Ast& ast,
      arangodb::aql::AstNode& node,
      transaction::Methods& trx,
      aql::ExpressionContext& ctx
  ) noexcept {
    _plan = &plan;
    _ast = &ast;
    _node.reset(&node, [](arangodb::aql::AstNode*){});
    _trx = &trx;
    _ctx = &ctx;
  }

  void init(
      aql::ExecutionPlan& plan,
      aql::Ast& ast,
      std::shared_ptr<arangodb::aql::AstNode>&& node,
      transaction::Methods& trx,
      aql::ExpressionContext& ctx
  ) noexcept {
    _plan = &plan;
    _ast = &ast;
    _node = std::move(node);
    _trx = &trx;
    _ctx = &ctx;
  }

  using irs::filter::prepare;

  virtual irs::filter::prepared::ptr prepare(
    irs::index_reader const& index,
    irs::order::prepared const& ord,
    irs::boost::boost_t boost
  ) const override;

  virtual size_t hash() const override;

  explicit operator bool() const noexcept {
    return bool(_node);
  }

 protected:
  virtual bool equals(irs::filter const& rhs) const override;

 private:
  aql::ExecutionPlan* _plan{};
  aql::Ast* _ast{};
  std::shared_ptr<arangodb::aql::AstNode> _node;
  transaction::Methods* _trx{}; // FIXME: do not store it here, pass to prepared::execute
  aql::ExpressionContext* _ctx{}; // // FIXME: do not store it here, pass to prepared::execute
}; // ByExpression

NS_END // iresearch
NS_END // arangodb

#endif // ARANGODB_IRESEARCH__IRESEARCH_EXPRESSION_FILTER

