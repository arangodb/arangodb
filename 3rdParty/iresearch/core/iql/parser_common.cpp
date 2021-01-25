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

#include "parser_common.hpp"

namespace {

static const iresearch::iql::function_arg::fn_value_t NOT_IMPLEMENTED_VALUE = [](
  iresearch::bstring&,
  const std::locale&,
  void* const&,
  const std::vector<iresearch::iql::function_arg>&
)->bool {
  return false;
};

}

using namespace iresearch::iql;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private members
// -----------------------------------------------------------------------------

namespace {
  const boolean_functions BOOLEAN_FUNCTIONS; // empty set of boolean_fuctions
  const order_functions ORDER_FUNCTIONS; // empty set of sequence_functions
  const sequence_functions SEQUENCE_FUNCTIONS; // default set of order_functions
}

/*static*/ decltype(function_arg::NOT_IMPLEMENTED_BRANCH) function_arg::NOT_IMPLEMENTED_BRANCH =
  [](proxy_filter&, const std::locale&, void* const&, const std::vector<function_arg>&)->bool { return false; };

// -----------------------------------------------------------------------------
// --SECTION--                                                      constructors
// -----------------------------------------------------------------------------

function_arg::function_arg(
  fn_args_t&& fnArgs,
  const fn_value_t& fnValue,
  const fn_branch_t& fnBranch /*= NOT_IMPLEMENTED_BRANCH*/
):
  m_fnArgs(std::move(fnArgs)),
  m_bFnBranchRef(true),
  m_bFnValueRef(true),
  m_bValueNil(false),
  m_pFnBranch(&fnBranch),
  m_pFnValue(&fnValue) {
}

function_arg::function_arg(fn_args_t&& fnArgs, const fn_branch_t& fnBranch):
  function_arg(std::move(fnArgs), NOT_IMPLEMENTED_VALUE, fnBranch) {
}

function_arg::function_arg(
  fn_args_t&& fnArgs, const fn_value_t& fnValue, fn_branch_t&& fnBranch
):
  m_fnArgs(std::move(fnArgs)),
  m_bFnBranchRef(false),
  m_bFnValueRef(true),
  m_bValueNil(false),
  m_pFnValue(&fnValue),
  m_fnBranch(std::move(fnBranch)) {
  m_pFnBranch = &m_fnBranch;
}

function_arg::function_arg(
  fn_args_t&& fnArgs, const fn_value_t&& fnValue, fn_branch_t& fnBranch
):
  m_fnArgs(std::move(fnArgs)),
  m_bFnBranchRef(true),
  m_bFnValueRef(false),
  m_bValueNil(false),
  m_pFnBranch(&fnBranch),
  m_fnValue(std::move(fnValue)) {
  m_pFnValue = &m_fnValue;
}

function_arg::function_arg(
  fn_args_t&& fnArgs, fn_value_t&& fnValue, fn_branch_t&& fnBranch
):
  m_fnArgs(std::move(fnArgs)),
  m_bFnBranchRef(false),
  m_bFnValueRef(false),
  m_bValueNil(false),
  m_fnBranch(std::move(fnBranch)),
  m_fnValue(std::move(fnValue)) {
  m_pFnBranch = &m_fnBranch;
  m_pFnValue = &m_fnValue;
}

function_arg::function_arg(fn_args_t&& fnArgs, fn_branch_t&& fnBranch):
  m_fnArgs(std::move(fnArgs)),
  m_bFnBranchRef(false),
  m_bFnValueRef(true),
  m_bValueNil(false),
  m_pFnValue(&NOT_IMPLEMENTED_VALUE),
  m_fnBranch(std::move(fnBranch)) {
  m_pFnBranch = &m_fnBranch;
}

function_arg::function_arg(
  fn_args_t&& fnArgs,
  const iresearch::bytes_ref& value,
  const fn_branch_t& fnBranch
):
  m_fnArgs(std::move(fnArgs)),
  m_bFnBranchRef(true),
  m_bFnValueRef(false),
  m_bValueNil(value.null()),
  m_pFnBranch(&fnBranch) {
  m_fnValue = [value](
    bstring& buf,
    const std::locale&,
    const void*,
    const std::vector<function_arg>& args
  )->bool {
    if (!args.empty()) {
      return false;
    };

    buf.append(value);

    return true;
  };
  m_pFnValue = &m_fnValue;
}

function_arg::function_arg(
  fn_args_t&& fnArgs, const iresearch::bytes_ref& value, fn_branch_t&& fnBranch
):
  m_fnArgs(std::move(fnArgs)),
  m_bFnBranchRef(false),
  m_bFnValueRef(false),
  m_bValueNil(value.null()),
  m_fnBranch(std::move(fnBranch)) {
  m_pFnBranch = &m_fnBranch;
  m_fnValue = [value](
    bstring& buf,
    const std::locale&,
    const void*,
    const std::vector<function_arg>& args
  )->bool {
    if (!args.empty()) {
      return false;
    };

    buf.append(value);

    return true;
  };
  m_pFnValue = &m_fnValue;
}

function_arg::function_arg(const iresearch::bytes_ref& value):
  function_arg(fn_args_t(), value, NOT_IMPLEMENTED_BRANCH) {
}

function_arg::function_arg(function_arg&& other) noexcept
  : m_fnArgs(std::move(other.m_fnArgs)),
    m_bFnBranchRef(std::move(other.m_bFnBranchRef)),
    m_bFnValueRef(std::move(other.m_bFnValueRef)),
    m_bValueNil(std::move(other.m_bValueNil)),
    m_fnBranch(std::move(other.m_fnBranch)),
    m_fnValue(std::move(other.m_fnValue)) {
  m_pFnBranch = m_bFnBranchRef ? std::move(other.m_pFnBranch) : &m_fnBranch;
  m_pFnValue = m_bFnValueRef ? std::move(other.m_pFnValue) : &m_fnValue;
}

function_arg::function_arg():
  function_arg(fn_args_t(), NOT_IMPLEMENTED_BRANCH) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public operators
// -----------------------------------------------------------------------------

function_arg& function_arg::operator=(function_arg&& other) noexcept {
  if (this != &other) {
    m_fnArgs = std::move(other.m_fnArgs);
    m_bFnBranchRef = std::move(other.m_bFnBranchRef);
    m_bFnValueRef = std::move(other.m_bFnValueRef);
    m_bValueNil = std::move(other.m_bValueNil);
    m_fnBranch = std::move(other.m_fnBranch);
    m_fnValue = std::move(other.m_fnValue);
    m_pFnBranch = m_bFnBranchRef ? std::move(other.m_pFnBranch) : &m_fnBranch;
    m_pFnValue = m_bFnValueRef ? std::move(other.m_pFnValue) : &m_fnValue;
  }

  return *this;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

bool function_arg::branch(
  proxy_filter& buf, const std::locale& locale, void* const& cookie
) const {
  return (*m_pFnBranch)(buf, locale, cookie, m_fnArgs);
}

bool function_arg::value(
  iresearch::bstring& buf,
  bool& bNil,
  const std::locale& locale,
  void* const& cookie
) const {
  bNil = m_bValueNil;
  return bNil || (*m_pFnValue)(buf, locale, cookie, m_fnArgs);
}

/*static*/ function_arg function_arg::wrap(const function_arg& wrapped) {
  return wrapped.m_bValueNil
    ? function_arg(fn_args_t(),
        iresearch::bytes_ref::NIL,
        [&wrapped](
          iresearch::iql::proxy_filter& buf,
          const std::locale& locale,
          void* const& cookie,
          const iresearch::iql::function_arg::fn_args_t&
        )->bool {
            return wrapped.branch(buf, locale, cookie);
        }
      )
    : function_arg(
        fn_args_t(),
        [&wrapped](
          bstring& buf,
          std::locale const& locale,
          void* const& cookie,
          const iresearch::iql::function_arg::fn_args_t&
        )->bool {
          bool bNil; // will always be false for function calls
          return wrapped.value(buf, bNil, locale, cookie);
        },
        [&wrapped](
          iresearch::iql::proxy_filter& buf,
          const std::locale& locale,
          void* const& cookie,
          const iresearch::iql::function_arg::fn_args_t&
        )->bool {
          return wrapped.branch(buf, locale, cookie);
        }
      )
    ;
}

/*static*/ const functions& functions::DEFAULT() {
  static const functions FUNCTIONS(
    BOOLEAN_FUNCTIONS,
    SEQUENCE_FUNCTIONS,
    ORDER_FUNCTIONS
  );

  return FUNCTIONS;
}
