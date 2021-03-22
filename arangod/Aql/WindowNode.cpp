////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "WindowNode.h"

#include "Aql/Ast.h"
#include "Aql/CountCollectExecutor.h"
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/QueryContext.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/WindowExecutor.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Value.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

WindowBounds::WindowBounds(Type type,
                           AqlValue&& preceding,
                           AqlValue&& following)
    : _type(type) {
  auto g = scopeGuard([&] {
    preceding.destroy();
    following.destroy();
  });
        
  if (Type::Row == type) {
    auto validate = [&](AqlValue& val) -> int64_t {
      if (val.isNumber()) {
        int64_t v = val.toInt64();
        if (v < 0) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                         "WINDOW row spec is invalid");
        }
        return v;
      }
      if (val.isString() && (val.slice().isEqualString("unbounded") ||
                             val.slice().isEqualString("inf"))) {
        return std::numeric_limits<int64_t>::max();
      }
      if (val.isNone()) {
        return 0;
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "WINDOW row spec is invalid");
    };
    _numPrecedingRows = validate(preceding);
    _numFollowingRows = validate(following);
    return;
  }
      
  if (Type::Range == type) {
    if (!(preceding.isString() && following.isString()) &&
        !(preceding.isNumber() && following.isNumber())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                                     "WINDOW range spec is invalid");
    }

    if (preceding.isString()) {
      _rangeType = RangeType::Date;
      TRI_ASSERT(following.isString());
      if (!basics::parseIsoDuration(preceding.slice().stringRef(), _precedingDuration)) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "WINDOW bound 'preceding' is not a "
                                       "valid ISO 8601 duration string");
      }
      if (!basics::parseIsoDuration(following.slice().stringRef(), _followingDuration)) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "WINDOW bound 'following' is not a "
                                       "valid ISO 8601 duration string");
      }
      return;
    }
    _rangeType = RangeType::Numeric;
    _precedingNumber = preceding.toDouble();
    _followingNumber = following.toDouble();
  }
}

WindowBounds::WindowBounds(Type t, VPackSlice slice)
  : WindowBounds(t, AqlValue(slice.get("following")),
                 AqlValue(slice.get("preceding"))) {}

WindowBounds::~WindowBounds() = default;

int64_t WindowBounds::numPrecedingRows() const {
  TRI_ASSERT(_type == Type::Row);
  return _numPrecedingRows;
}

int64_t WindowBounds::numFollowingRows() const {
  TRI_ASSERT(_type == Type::Row);
  return _numFollowingRows;
}

bool WindowBounds::unboundedPreceding() const {
  return _type == Type::Row &&
         _numPrecedingRows == std::numeric_limits<int64_t>::max() &&
         _numFollowingRows == 0;
}

bool WindowBounds::needsFollowingRows() const {
  if (_type == Type::Row) {
    return _numPrecedingRows == std::numeric_limits<int64_t>::max() &&
           _numFollowingRows == 0;
  }
  TRI_ASSERT(_type == Type::Range);
  if (_rangeType == RangeType::Date) {
    return _followingDuration.years > 0 || _followingDuration.months > 0 ||
           _followingDuration.days > 0 || _followingDuration.hours > 0 ||
           _followingDuration.minutes > 0 || _followingDuration.seconds > 0 ||
           _followingDuration.milliseconds > 0;
  }
  TRI_ASSERT(_rangeType == RangeType::Numeric)
  return _followingNumber > 0.0;
}

namespace {
bool parameterToTimePoint(AqlValue const& value, QueryWarnings& warnings,
                          tp_sys_clock_ms& tp) {
  if (value.isNumber()) {
    int64_t v = value.toInt64();
    if (ADB_UNLIKELY(v < -62167219200000 || v > 253402300799999)) {
      // check if value is between "0000-01-01T00:00:00.000Z" and
      // "9999-12-31T23:59:59.999Z" -62167219200000: "0000-01-01T00:00:00.000Z"
      // 253402300799999: "9999-12-31T23:59:59.999Z"
      warnings.registerWarning(
          TRI_ERROR_QUERY_INVALID_DATE_VALUE,
          "range value is not a valid timepoint (out of range)");
      return false;
    }
    tp = tp_sys_clock_ms(std::chrono::milliseconds(v));
    return true;
  }
  // TODO is there a way to properly support ISO datestrings ?
  /*else if (value.isString()) {
    if (!basics::parseDateTime(value.slice().stringRef(), tp)) {
      q.warnings().registerWarning(TRI_ERROR_QUERY_INVALID_DATE_VALUE,
                                   "range value is not a valid ISO 8601 date
  time string"); return false;
    }
    return true;
  }*/
  warnings.registerWarning(TRI_ERROR_QUERY_INVALID_DATE_VALUE);
  return false;
}

tp_sys_clock_ms addOrSubtractDate(tp_sys_clock_ms tp,
                                  arangodb::basics::ParsedDuration const& parsed,
                                  bool isSubtract) {
  date::year_month_day ymd{date::floor<date::days>(tp)};
  auto day_time = date::make_time(tp - date::sys_days(ymd));

  if (isSubtract) {
    ymd -= date::years{parsed.years};
  } else {
    ymd += date::years{parsed.years};
  }

  if (isSubtract) {
    ymd -= date::months{parsed.months};
  } else {
    ymd += date::months{parsed.months};
  }

  std::chrono::milliseconds ms{0};
  ms += date::weeks{parsed.weeks};
  ms += date::days{parsed.days};
  ms += std::chrono::hours{parsed.hours};
  ms += std::chrono::minutes{parsed.minutes};
  ms += std::chrono::seconds{parsed.seconds};
  ms += std::chrono::milliseconds{parsed.milliseconds};

  if (isSubtract) {
    return tp_sys_clock_ms{date::sys_days(ymd) + day_time.to_duration() - ms};
  } else {
    return tp_sys_clock_ms{date::sys_days(ymd) + day_time.to_duration() + ms};
  }
}
}  // namespace

WindowBounds::Row WindowBounds::calcRow(AqlValue const& input, QueryWarnings& w) const {
  using namespace date;
  TRI_ASSERT(_type == Type::Range);

  if (_rangeType == RangeType::Date) {
    tp_sys_clock_ms tp;
    if (!::parameterToTimePoint(input, w, tp)) {
      return {0, 0, 0, /*valid*/ false};
    }

    auto lowerTP = addOrSubtractDate(tp, _precedingDuration, /*subtract*/ true);
    auto upperTP = addOrSubtractDate(tp, _followingDuration, /*subtract*/ false);

    auto val = std::chrono::duration<double>(tp.time_since_epoch()).count();
    auto low = std::chrono::duration<double>(lowerTP.time_since_epoch()).count();
    auto upper = std::chrono::duration<double>(upperTP.time_since_epoch()).count();

    return {val, low, upper, /*valid*/ true};
  }

  TRI_ASSERT(_rangeType == RangeType::Numeric);

  bool failed;
  double val = input.toDouble(failed);
  if (failed) {
    w.registerWarning(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE);
    return {0, 0, 0, /*valid*/ false};
  }

  return {val, val - _precedingNumber, val + _followingNumber, /*valid*/ true};
}

void WindowBounds::toVelocyPack(VPackBuilder& b) const {
  switch (_type) {
    case Type::Row:
      b.add("preceding", VPackValue(_numPrecedingRows));
      b.add("following", VPackValue(_numFollowingRows));
      break;
      
    case Type::Range:
      if (_rangeType == RangeType::Numeric) {
        b.add("preceding", VPackValue(_precedingNumber));
        b.add("following", VPackValue(_followingNumber));
      } else {
        auto makeDuration = [](basics::ParsedDuration const& duration) {
          std::string p = "P";
          p.append(std::to_string(duration.years)).append("Y");
          p.append(std::to_string(duration.months)).append("M");
          p.append(std::to_string(duration.days)).append("DT");
          p.append(std::to_string(duration.hours)).append("H");
          p.append(std::to_string(duration.minutes)).append("M");
          p.append(std::to_string(duration.seconds)).append(".");
          p.append(std::to_string(duration.milliseconds)).append("S");
          return p;
        };
        b.add("preceding", VPackValue(makeDuration(_precedingDuration)));
        b.add("following", VPackValue(makeDuration(_followingDuration)));
      }
      break;
  }
}

WindowNode::WindowNode(ExecutionPlan* plan, ExecutionNodeId id,
                       WindowBounds&& b, Variable const* rangeVariable,
                       std::vector<AggregateVarInfo> const& aggregateVariables)
    : ExecutionNode(plan, id),
      _bounds(std::move(b)),
      _rangeVariable(rangeVariable),
      _aggregateVariables(aggregateVariables) {}

WindowNode::WindowNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base,
                       WindowBounds&& b, Variable const* rangeVariable,
                       std::vector<AggregateVarInfo> const& aggregateVariables)
    : ExecutionNode(plan, base),
      _bounds(std::move(b)),
      _rangeVariable(rangeVariable),
      _aggregateVariables(aggregateVariables) {}

WindowNode::~WindowNode() = default;

/// @brief toVelocyPack, for CollectNode
void WindowNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                    std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

  if (_rangeVariable) {
    nodes.add(VPackValue("rangeVariable"));
    _rangeVariable->toVelocyPack(nodes);
  }

  // aggregate variables
  nodes.add(VPackValue("aggregates"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& aggregateVariable : _aggregateVariables) {
      VPackObjectBuilder obj(&nodes);
      nodes.add(VPackValue("outVariable"));
      aggregateVariable.outVar->toVelocyPack(nodes);
      nodes.add(VPackValue("inVariable"));
      aggregateVariable.inVar->toVelocyPack(nodes);
      nodes.add("type", VPackValue(aggregateVariable.type));
    }
  }

  _bounds.toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

void WindowNode::calcAggregateRegisters(std::vector<std::pair<RegisterId, RegisterId>>& aggregateRegisters,
                                        RegIdSet& readableInputRegisters,
                                        RegIdSet& writeableOutputRegisters) const {
  for (auto const& p : _aggregateVariables) {
    // We know that planRegisters() has been run, so
    // getPlanNode()->_registerPlan is set up
    auto itOut = getRegisterPlan()->varInfo.find(p.outVar->id);
    TRI_ASSERT(itOut != getRegisterPlan()->varInfo.end());
    RegisterId outReg = itOut->second.registerId;
    TRI_ASSERT(outReg.isValid());

    RegisterId inReg = RegisterPlan::MaxRegisterId;
    if (Aggregator::requiresInput(p.type)) {
      auto itIn = getRegisterPlan()->varInfo.find(p.inVar->id);
      TRI_ASSERT(itIn != getRegisterPlan()->varInfo.end());
      inReg = itIn->second.registerId;
      TRI_ASSERT(inReg.isValid());
      readableInputRegisters.insert(inReg);
    }
    // else: no input variable required

    aggregateRegisters.emplace_back(std::make_pair(outReg, inReg));
    writeableOutputRegisters.insert((outReg));
  }
  TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());
}

void WindowNode::calcAggregateTypes(std::vector<std::unique_ptr<Aggregator>>& aggregateTypes) const {
  for (auto const& p : _aggregateVariables) {
    aggregateTypes.emplace_back(
        Aggregator::fromTypeString(&_plan->getAst()->query().vpackOptions(), p.type));
  }
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> WindowNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);

  RegIdSet readableInputRegisters;
  RegIdSet writeableOutputRegisters;

  RegisterId rangeRegister = RegisterPlan::MaxRegisterId;
  if (_rangeVariable != nullptr) {
    auto itIn = getRegisterPlan()->varInfo.find(_rangeVariable->id);
    TRI_ASSERT(itIn != getRegisterPlan()->varInfo.end());
    rangeRegister = itIn->second.registerId;
    TRI_ASSERT(rangeRegister.isValid());
    readableInputRegisters.insert(rangeRegister);
  }

  // calculate the aggregate registers
  std::vector<std::pair<RegisterId, RegisterId>> aggregateRegisters;
  calcAggregateRegisters(aggregateRegisters, readableInputRegisters, writeableOutputRegisters);

  TRI_ASSERT(aggregateRegisters.size() == _aggregateVariables.size());

  auto registerInfos = createRegisterInfos(std::move(readableInputRegisters),
                                           std::move(writeableOutputRegisters));

  std::vector<std::string> aggregateTypes;
  std::transform(_aggregateVariables.begin(), _aggregateVariables.end(),
                 std::back_inserter(aggregateTypes),
                 [](auto& it) { return it.type; });
  TRI_ASSERT(aggregateTypes.size() == _aggregateVariables.size());

  auto executorInfos =
      WindowExecutorInfos(_bounds, rangeRegister, std::move(aggregateTypes),
                          std::move(aggregateRegisters), engine.getQuery().warnings(),
                          &_plan->getAst()->query().vpackOptions());

  if (_rangeVariable == nullptr && _bounds.unboundedPreceding()) {
    return std::make_unique<ExecutionBlockImpl<AccuWindowExecutor>>(
        &engine, this, std::move(registerInfos), std::move(executorInfos));
  }
  return std::make_unique<ExecutionBlockImpl<WindowExecutor>>(&engine, this,
                                                              std::move(registerInfos),
                                                              std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* WindowNode::clone(ExecutionPlan* plan, bool withDependencies,
                                 bool withProperties) const {
  auto aggregateVariables = _aggregateVariables;

  if (withProperties) {
    // need to re-create all variables

    aggregateVariables.clear();

    for (auto& it : _aggregateVariables) {
      auto out = plan->getAst()->variables()->createVariable(it.outVar);
      auto in = plan->getAst()->variables()->createVariable(it.inVar);
      aggregateVariables.emplace_back(AggregateVarInfo{out, in, it.type});
    }
  }

  auto c = std::make_unique<WindowNode>(plan, _id, WindowBounds(_bounds),
                                        _rangeVariable, aggregateVariables);

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

/// @brief getVariablesUsedHere, modifying the set in-place
void WindowNode::getVariablesUsedHere(VarSet& vars) const {
  if (_rangeVariable) {
    vars.emplace(_rangeVariable);
  }
  for (auto const& p : _aggregateVariables) {
    vars.emplace(p.inVar);
  }
}

void WindowNode::setAggregateVariables(std::vector<AggregateVarInfo> const& aggregateVariables) {
  _aggregateVariables = aggregateVariables;
}

/// @brief estimateCost
CostEstimate WindowNode::estimateCost() const {
  // we never return more rows than above
  CostEstimate estimate = _dependencies.at(0)->getCost();
  if (_rangeVariable == nullptr) {
    int64_t numRows = 1 + _bounds.numPrecedingRows() + _bounds.numFollowingRows();
    estimate.estimatedCost += double(numRows * numRows) * _aggregateVariables.size();
  } else {  // guestimate
    estimate.estimatedCost += 4 * _aggregateVariables.size();
  }
  return estimate;
}

ExecutionNode::NodeType WindowNode::getType() const {
  return ExecutionNode::WINDOW;
}

std::vector<Variable const*> WindowNode::getVariablesSetHere() const {
  std::vector<Variable const*> v;
  v.reserve(_aggregateVariables.size());
  for (auto const& p : _aggregateVariables) {
    v.emplace_back(p.outVar);
  }
  return v;
}

// does this WINDOW need to look at rows following the current one
bool WindowNode::needsFollowingRows() const {
  return _bounds.needsFollowingRows();
}
