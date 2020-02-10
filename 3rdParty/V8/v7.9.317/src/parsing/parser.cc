// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parser.h"

#include <algorithm>
#include <memory>

#include "src/ast/ast-function-literal-id-reindexer.h"
#include "src/ast/ast-traversal-visitor.h"
#include "src/ast/ast.h"
#include "src/ast/source-range-ast-visitor.h"
#include "src/base/ieee754.h"
#include "src/base/overflowing-math.h"
#include "src/base/platform/platform.h"
#include "src/codegen/bailout-reason.h"
#include "src/common/message-template.h"
#include "src/compiler-dispatcher/compiler-dispatcher.h"
#include "src/logging/log.h"
#include "src/numbers/conversions-inl.h"
#include "src/objects/scope-info.h"
#include "src/parsing/expression-scope-reparenter.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/rewriter.h"
#include "src/runtime/runtime.h"
#include "src/strings/char-predicates-inl.h"
#include "src/strings/string-stream.h"
#include "src/tracing/trace-event.h"
#include "src/zone/zone-list-inl.h"

namespace v8 {
namespace internal {

FunctionLiteral* Parser::DefaultConstructor(const AstRawString* name,
                                            bool call_super, int pos,
                                            int end_pos) {
  int expected_property_count = 0;
  const int parameter_count = 0;

  FunctionKind kind = call_super ? FunctionKind::kDefaultDerivedConstructor
                                 : FunctionKind::kDefaultBaseConstructor;
  DeclarationScope* function_scope = NewFunctionScope(kind);
  SetLanguageMode(function_scope, LanguageMode::kStrict);
  // Set start and end position to the same value
  function_scope->set_start_position(pos);
  function_scope->set_end_position(pos);
  ScopedPtrList<Statement> body(pointer_buffer());

  {
    FunctionState function_state(&function_state_, &scope_, function_scope);

    if (call_super) {
      // Create a SuperCallReference and handle in BytecodeGenerator.
      auto constructor_args_name = ast_value_factory()->empty_string();
      bool is_rest = true;
      bool is_optional = false;
      Variable* constructor_args = function_scope->DeclareParameter(
          constructor_args_name, VariableMode::kTemporary, is_optional, is_rest,
          ast_value_factory(), pos);

      Expression* call;
      {
        ScopedPtrList<Expression> args(pointer_buffer());
        Spread* spread_args = factory()->NewSpread(
            factory()->NewVariableProxy(constructor_args), pos, pos);

        args.Add(spread_args);
        Expression* super_call_ref = NewSuperCallReference(pos);
        call = factory()->NewCall(super_call_ref, args, pos);
      }
      body.Add(factory()->NewReturnStatement(call, pos));
    }

    expected_property_count = function_state.expected_property_count();
  }

  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      name, function_scope, body, expected_property_count, parameter_count,
      parameter_count, FunctionLiteral::kNoDuplicateParameters,
      FunctionSyntaxKind::kAnonymousExpression, default_eager_compile_hint(),
      pos, true, GetNextFunctionLiteralId());
  return function_literal;
}

void Parser::ReportUnexpectedTokenAt(Scanner::Location location,
                                     Token::Value token,
                                     MessageTemplate message) {
  const char* arg = nullptr;
  switch (token) {
    case Token::EOS:
      message = MessageTemplate::kUnexpectedEOS;
      break;
    case Token::SMI:
    case Token::NUMBER:
    case Token::BIGINT:
      message = MessageTemplate::kUnexpectedTokenNumber;
      break;
    case Token::STRING:
      message = MessageTemplate::kUnexpectedTokenString;
      break;
    case Token::PRIVATE_NAME:
    case Token::IDENTIFIER:
      message = MessageTemplate::kUnexpectedTokenIdentifier;
      break;
    case Token::AWAIT:
    case Token::ENUM:
      message = MessageTemplate::kUnexpectedReserved;
      break;
    case Token::LET:
    case Token::STATIC:
    case Token::YIELD:
    case Token::FUTURE_STRICT_RESERVED_WORD:
      message = is_strict(language_mode())
                    ? MessageTemplate::kUnexpectedStrictReserved
                    : MessageTemplate::kUnexpectedTokenIdentifier;
      break;
    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      message = MessageTemplate::kUnexpectedTemplateString;
      break;
    case Token::ESCAPED_STRICT_RESERVED_WORD:
    case Token::ESCAPED_KEYWORD:
      message = MessageTemplate::kInvalidEscapedReservedWord;
      break;
    case Token::ILLEGAL:
      if (scanner()->has_error()) {
        message = scanner()->error();
        location = scanner()->error_location();
      } else {
        message = MessageTemplate::kInvalidOrUnexpectedToken;
      }
      break;
    case Token::REGEXP_LITERAL:
      message = MessageTemplate::kUnexpectedTokenRegExp;
      break;
    default:
      const char* name = Token::String(token);
      DCHECK_NOT_NULL(name);
      arg = name;
      break;
  }
  ReportMessageAt(location, message, arg);
}

// ----------------------------------------------------------------------------
// Implementation of Parser

bool Parser::ShortcutNumericLiteralBinaryExpression(Expression** x,
                                                    Expression* y,
                                                    Token::Value op, int pos) {
  if ((*x)->IsNumberLiteral() && y->IsNumberLiteral()) {
    double x_val = (*x)->AsLiteral()->AsNumber();
    double y_val = y->AsLiteral()->AsNumber();
    switch (op) {
      case Token::ADD:
        *x = factory()->NewNumberLiteral(x_val + y_val, pos);
        return true;
      case Token::SUB:
        *x = factory()->NewNumberLiteral(x_val - y_val, pos);
        return true;
      case Token::MUL:
        *x = factory()->NewNumberLiteral(x_val * y_val, pos);
        return true;
      case Token::DIV:
        *x = factory()->NewNumberLiteral(base::Divide(x_val, y_val), pos);
        return true;
      case Token::BIT_OR: {
        int value = DoubleToInt32(x_val) | DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::BIT_AND: {
        int value = DoubleToInt32(x_val) & DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::BIT_XOR: {
        int value = DoubleToInt32(x_val) ^ DoubleToInt32(y_val);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SHL: {
        int value =
            base::ShlWithWraparound(DoubleToInt32(x_val), DoubleToInt32(y_val));
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SHR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1F;
        uint32_t value = DoubleToUint32(x_val) >> shift;
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::SAR: {
        uint32_t shift = DoubleToInt32(y_val) & 0x1F;
        int value = ArithmeticShiftRight(DoubleToInt32(x_val), shift);
        *x = factory()->NewNumberLiteral(value, pos);
        return true;
      }
      case Token::EXP:
        *x = factory()->NewNumberLiteral(base::ieee754::pow(x_val, y_val), pos);
        return true;
      default:
        break;
    }
  }
  return false;
}

bool Parser::CollapseNaryExpression(Expression** x, Expression* y,
                                    Token::Value op, int pos,
                                    const SourceRange& range) {
  // Filter out unsupported ops.
  if (!Token::IsBinaryOp(op) || op == Token::EXP) return false;

  // Convert *x into an nary operation with the given op, returning false if
  // this is not possible.
  NaryOperation* nary = nullptr;
  if ((*x)->IsBinaryOperation()) {
    BinaryOperation* binop = (*x)->AsBinaryOperation();
    if (binop->op() != op) return false;

    nary = factory()->NewNaryOperation(op, binop->left(), 2);
    nary->AddSubsequent(binop->right(), binop->position());
    ConvertBinaryToNaryOperationSourceRange(binop, nary);
    *x = nary;
  } else if ((*x)->IsNaryOperation()) {
    nary = (*x)->AsNaryOperation();
    if (nary->op() != op) return false;
  } else {
    return false;
  }

  // Append our current expression to the nary operation.
  // TODO(leszeks): Do some literal collapsing here if we're appending Smi or
  // String literals.
  nary->AddSubsequent(y, pos);
  nary->clear_parenthesized();
  AppendNaryOperationSourceRange(nary, range);

  return true;
}

Expression* Parser::BuildUnaryExpression(Expression* expression,
                                         Token::Value op, int pos) {
  DCHECK_NOT_NULL(expression);
  const Literal* literal = expression->AsLiteral();
  if (literal != nullptr) {
    if (op == Token::NOT) {
      // Convert the literal to a boolean condition and negate it.
      return factory()->NewBooleanLiteral(literal->ToBooleanIsFalse(), pos);
    } else if (literal->IsNumberLiteral()) {
      // Compute some expressions involving only number literals.
      double value = literal->AsNumber();
      switch (op) {
        case Token::ADD:
          return expression;
        case Token::SUB:
          return factory()->NewNumberLiteral(-value, pos);
        case Token::BIT_NOT:
          return factory()->NewNumberLiteral(~DoubleToInt32(value), pos);
        default:
          break;
      }
    }
  }
  return factory()->NewUnaryOperation(op, expression, pos);
}

Expression* Parser::NewThrowError(Runtime::FunctionId id,
                                  MessageTemplate message,
                                  const AstRawString* arg, int pos) {
  ScopedPtrList<Expression> args(pointer_buffer());
  args.Add(factory()->NewSmiLiteral(static_cast<int>(message), pos));
  args.Add(factory()->NewStringLiteral(arg, pos));
  CallRuntime* call_constructor = factory()->NewCallRuntime(id, args, pos);
  return factory()->NewThrow(call_constructor, pos);
}

Expression* Parser::NewSuperPropertyReference(int pos) {
  // this_function[home_object_symbol]
  VariableProxy* this_function_proxy =
      NewUnresolved(ast_value_factory()->this_function_string(), pos);
  Expression* home_object_symbol_literal = factory()->NewSymbolLiteral(
      AstSymbol::kHomeObjectSymbol, kNoSourcePosition);
  Expression* home_object = factory()->NewProperty(
      this_function_proxy, home_object_symbol_literal, pos);
  return factory()->NewSuperPropertyReference(home_object, pos);
}

Expression* Parser::NewSuperCallReference(int pos) {
  VariableProxy* new_target_proxy =
      NewUnresolved(ast_value_factory()->new_target_string(), pos);
  VariableProxy* this_function_proxy =
      NewUnresolved(ast_value_factory()->this_function_string(), pos);
  return factory()->NewSuperCallReference(new_target_proxy, this_function_proxy,
                                          pos);
}

Expression* Parser::NewTargetExpression(int pos) {
  auto proxy = NewUnresolved(ast_value_factory()->new_target_string(), pos);
  proxy->set_is_new_target();
  return proxy;
}

Expression* Parser::ImportMetaExpression(int pos) {
  ScopedPtrList<Expression> args(pointer_buffer());
  return factory()->NewCallRuntime(Runtime::kInlineGetImportMetaObject, args,
                                   pos);
}

Expression* Parser::ExpressionFromLiteral(Token::Value token, int pos) {
  switch (token) {
    case Token::NULL_LITERAL:
      return factory()->NewNullLiteral(pos);
    case Token::TRUE_LITERAL:
      return factory()->NewBooleanLiteral(true, pos);
    case Token::FALSE_LITERAL:
      return factory()->NewBooleanLiteral(false, pos);
    case Token::SMI: {
      uint32_t value = scanner()->smi_value();
      return factory()->NewSmiLiteral(value, pos);
    }
    case Token::NUMBER: {
      double value = scanner()->DoubleValue();
      return factory()->NewNumberLiteral(value, pos);
    }
    case Token::BIGINT:
      return factory()->NewBigIntLiteral(
          AstBigInt(scanner()->CurrentLiteralAsCString(zone())), pos);
    case Token::STRING: {
      return factory()->NewStringLiteral(GetSymbol(), pos);
    }
    default:
      DCHECK(false);
  }
  return FailureExpression();
}

Expression* Parser::NewV8Intrinsic(const AstRawString* name,
                                   const ScopedPtrList<Expression>& args,
                                   int pos) {
  if (extension_ != nullptr) {
    // The extension structures are only accessible while parsing the
    // very first time, not when reparsing because of lazy compilation.
    GetClosureScope()->ForceEagerCompilation();
  }

  if (!name->is_one_byte()) {
    // There are no two-byte named intrinsics.
    ReportMessage(MessageTemplate::kNotDefined, name);
    return FailureExpression();
  }

  const Runtime::Function* function =
      Runtime::FunctionForName(name->raw_data(), name->length());

  if (function != nullptr) {
    // Check for possible name clash.
    DCHECK_EQ(Context::kNotFound,
              Context::IntrinsicIndexForName(name->raw_data(), name->length()));

    // Check that the expected number of arguments are being passed.
    if (function->nargs != -1 && function->nargs != args.length()) {
      ReportMessage(MessageTemplate::kRuntimeWrongNumArgs);
      return FailureExpression();
    }

    return factory()->NewCallRuntime(function, args, pos);
  }

  int context_index =
      Context::IntrinsicIndexForName(name->raw_data(), name->length());

  // Check that the function is defined.
  if (context_index == Context::kNotFound) {
    ReportMessage(MessageTemplate::kNotDefined, name);
    return FailureExpression();
  }

  return factory()->NewCallRuntime(context_index, args, pos);
}

Parser::Parser(ParseInfo* info)
    : ParserBase<Parser>(info->zone(), &scanner_, info->stack_limit(),
                         info->extension(), info->GetOrCreateAstValueFactory(),
                         info->pending_error_handler(),
                         info->runtime_call_stats(), info->logger(),
                         info->script().is_null() ? -1 : info->script()->id(),
                         info->is_module(), true),
      info_(info),
      scanner_(info->character_stream(), info->is_module()),
      preparser_zone_(info->zone()->allocator(), ZONE_NAME),
      reusable_preparser_(nullptr),
      mode_(PARSE_EAGERLY),  // Lazy mode must be set explicitly.
      source_range_map_(info->source_range_map()),
      target_stack_(nullptr),
      total_preparse_skipped_(0),
      consumed_preparse_data_(info->consumed_preparse_data()),
      preparse_data_buffer_(),
      parameters_end_pos_(info->parameters_end_pos()) {
  // Even though we were passed ParseInfo, we should not store it in
  // Parser - this makes sure that Isolate is not accidentally accessed via
  // ParseInfo during background parsing.
  DCHECK_NOT_NULL(info->character_stream());
  // Determine if functions can be lazily compiled. This is necessary to
  // allow some of our builtin JS files to be lazily compiled. These
  // builtins cannot be handled lazily by the parser, since we have to know
  // if a function uses the special natives syntax, which is something the
  // parser records.
  // If the debugger requests compilation for break points, we cannot be
  // aggressive about lazy compilation, because it might trigger compilation
  // of functions without an outer context when setting a breakpoint through
  // Debug::FindSharedFunctionInfoInScript
  // We also compile eagerly for kProduceExhaustiveCodeCache.
  bool can_compile_lazily = info->allow_lazy_compile() && !info->is_eager();

  set_default_eager_compile_hint(can_compile_lazily
                                     ? FunctionLiteral::kShouldLazyCompile
                                     : FunctionLiteral::kShouldEagerCompile);
  allow_lazy_ = info->allow_lazy_compile() && info->allow_lazy_parsing() &&
                info->extension() == nullptr && can_compile_lazily;
  set_allow_natives(info->allow_natives_syntax());
  set_allow_harmony_dynamic_import(info->allow_harmony_dynamic_import());
  set_allow_harmony_import_meta(info->allow_harmony_import_meta());
  set_allow_harmony_nullish(info->allow_harmony_nullish());
  set_allow_harmony_optional_chaining(info->allow_harmony_optional_chaining());
  set_allow_harmony_private_methods(info->allow_harmony_private_methods());
  set_allow_harmony_top_level_await(info->allow_harmony_top_level_await());
  for (int feature = 0; feature < v8::Isolate::kUseCounterFeatureCount;
       ++feature) {
    use_counts_[feature] = 0;
  }
}

void Parser::InitializeEmptyScopeChain(ParseInfo* info) {
  DCHECK_NULL(original_scope_);
  DCHECK_NULL(info->script_scope());
  DeclarationScope* script_scope = NewScriptScope();
  info->set_script_scope(script_scope);
  original_scope_ = script_scope;
}

void Parser::DeserializeScopeChain(
    Isolate* isolate, ParseInfo* info,
    MaybeHandle<ScopeInfo> maybe_outer_scope_info,
    Scope::DeserializationMode mode) {
  InitializeEmptyScopeChain(info);
  Handle<ScopeInfo> outer_scope_info;
  if (maybe_outer_scope_info.ToHandle(&outer_scope_info)) {
    DCHECK_EQ(ThreadId::Current(), isolate->thread_id());
    original_scope_ = Scope::DeserializeScopeChain(
        isolate, zone(), *outer_scope_info, info->script_scope(),
        ast_value_factory(), mode);
    if (info->is_eval() || IsArrowFunction(info->function_kind())) {
      original_scope_->GetReceiverScope()->DeserializeReceiver(
          ast_value_factory());
    }
  }
}

namespace {

void MaybeResetCharacterStream(ParseInfo* info, FunctionLiteral* literal) {
  // Don't reset the character stream if there is an asm.js module since it will
  // be used again by the asm-parser.
  if (info->contains_asm_module()) {
    if (FLAG_stress_validate_asm) return;
    if (literal != nullptr && literal->scope()->ContainsAsmModule()) return;
  }
  info->ResetCharacterStream();
}

void MaybeProcessSourceRanges(ParseInfo* parse_info, Expression* root,
                              uintptr_t stack_limit_) {
  if (root != nullptr && parse_info->source_range_map() != nullptr) {
    SourceRangeAstVisitor visitor(stack_limit_, root,
                                  parse_info->source_range_map());
    visitor.Run();
  }
}

}  // namespace

FunctionLiteral* Parser::ParseProgram(Isolate* isolate, ParseInfo* info) {
  // TODO(bmeurer): We temporarily need to pass allow_nesting = true here,
  // see comment for HistogramTimerScope class.

  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);
  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_, info->is_eval()
                               ? RuntimeCallCounterId::kParseEval
                               : RuntimeCallCounterId::kParseProgram);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.ParseProgram");
  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();

  // Initialize parser state.
  DeserializeScopeChain(isolate, info, info->maybe_outer_scope_info(),
                        Scope::DeserializationMode::kIncludingVariables);

  scanner_.Initialize();
  scanner_.SkipHashBang();
  FunctionLiteral* result = DoParseProgram(isolate, info);
  MaybeResetCharacterStream(info, result);
  MaybeProcessSourceRanges(info, result, stack_limit_);

  HandleSourceURLComments(isolate, info->script());

  if (V8_UNLIKELY(FLAG_log_function_events) && result != nullptr) {
    double ms = timer.Elapsed().InMillisecondsF();
    const char* event_name = "parse-eval";
    Script script = *info->script();
    int start = -1;
    int end = -1;
    if (!info->is_eval()) {
      event_name = "parse-script";
      start = 0;
      end = String::cast(script.source()).length();
    }
    LOG(isolate, FunctionEvent(event_name, script.id(), ms, start, end, "", 0));
  }
  return result;
}

FunctionLiteral* Parser::DoParseProgram(Isolate* isolate, ParseInfo* info) {
  // Note that this function can be called from the main thread or from a
  // background thread. We should not access anything Isolate / heap dependent
  // via ParseInfo, and also not pass it forward. If not on the main thread
  // isolate will be nullptr.
  DCHECK_EQ(parsing_on_main_thread_, isolate != nullptr);
  DCHECK_NULL(scope_);
  DCHECK_NULL(target_stack_);

  ParsingModeScope mode(this, allow_lazy_ ? PARSE_LAZILY : PARSE_EAGERLY);
  ResetFunctionLiteralId();
  DCHECK(info->function_literal_id() == kFunctionLiteralIdTopLevel ||
         info->function_literal_id() == kFunctionLiteralIdInvalid);

  FunctionLiteral* result = nullptr;
  {
    Scope* outer = original_scope_;
    DCHECK_NOT_NULL(outer);
    if (info->is_eval()) {
      outer = NewEvalScope(outer);
    } else if (parsing_module_) {
      DCHECK_EQ(outer, info->script_scope());
      outer = NewModuleScope(info->script_scope());
    }

    DeclarationScope* scope = outer->AsDeclarationScope();
    scope->set_start_position(0);

    FunctionState function_state(&function_state_, &scope_, scope);
    ScopedPtrList<Statement> body(pointer_buffer());
    int beg_pos = scanner()->location().beg_pos;
    if (parsing_module_) {
      DCHECK(info->is_module());
      // Declare the special module parameter.
      auto name = ast_value_factory()->empty_string();
      bool is_rest = false;
      bool is_optional = false;
      VariableMode mode = VariableMode::kVar;
      bool was_added;
      scope->DeclareLocal(name, mode, PARAMETER_VARIABLE, &was_added,
                          Variable::DefaultInitializationFlag(mode));
      DCHECK(was_added);
      auto var = scope->DeclareParameter(name, VariableMode::kVar, is_optional,
                                         is_rest, ast_value_factory(), beg_pos);
      var->AllocateTo(VariableLocation::PARAMETER, 0);

      PrepareGeneratorVariables();
      Expression* initial_yield =
          BuildInitialYield(kNoSourcePosition, kGeneratorFunction);
      body.Add(
          factory()->NewExpressionStatement(initial_yield, kNoSourcePosition));
      if (allow_harmony_top_level_await()) {
        // First parse statements into a buffer. Then, if there was a
        // top level await, create an inner block and rewrite the body of the
        // module as an async function. Otherwise merge the statements back
        // into the main body.
        BlockT block = impl()->NullBlock();
        {
          StatementListT statements(pointer_buffer());
          ParseModuleItemList(&statements);
          // Modules will always have an initial yield. If there are any
          // additional suspends, i.e. awaits, then we treat the module as an
          // AsyncModule.
          if (function_state.suspend_count() > 1) {
            scope->set_is_async_module();
            block = factory()->NewBlock(true, statements);
          } else {
            statements.MergeInto(&body);
          }
        }
        if (IsAsyncModule(scope->function_kind())) {
          impl()->RewriteAsyncFunctionBody(
              &body, block, factory()->NewUndefinedLiteral(kNoSourcePosition));
        }
      } else {
        ParseModuleItemList(&body);
      }
      if (!has_error() &&
          !module()->Validate(this->scope()->AsModuleScope(),
                              pending_error_handler(), zone())) {
        scanner()->set_parser_error();
      }
    } else if (info->is_wrapped_as_function()) {
      ParseWrapped(isolate, info, &body, scope, zone());
    } else {
      // Don't count the mode in the use counters--give the program a chance
      // to enable script-wide strict mode below.
      this->scope()->SetLanguageMode(info->language_mode());
      ParseStatementList(&body, Token::EOS);
    }

    // The parser will peek but not consume EOS.  Our scope logically goes all
    // the way to the EOS, though.
    scope->set_end_position(peek_position());

    if (is_strict(language_mode())) {
      CheckStrictOctalLiteral(beg_pos, end_position());
    }
    if (is_sloppy(language_mode())) {
      // TODO(littledan): Function bindings on the global object that modify
      // pre-existing bindings should be made writable, enumerable and
      // nonconfigurable if possible, whereas this code will leave attributes
      // unchanged if the property already exists.
      InsertSloppyBlockFunctionVarBindings(scope);
    }
    // Internalize the ast strings in the case of eval so we can check for
    // conflicting var declarations with outer scope-info-backed scopes.
    if (info->is_eval()) {
      DCHECK(parsing_on_main_thread_);
      info->ast_value_factory()->Internalize(isolate);
    }
    CheckConflictingVarDeclarations(scope);

    if (info->parse_restriction() == ONLY_SINGLE_FUNCTION_LITERAL) {
      if (body.length() != 1 || !body.at(0)->IsExpressionStatement() ||
          !body.at(0)
               ->AsExpressionStatement()
               ->expression()
               ->IsFunctionLiteral()) {
        ReportMessage(MessageTemplate::kSingleFunctionLiteral);
      }
    }

    int parameter_count = parsing_module_ ? 1 : 0;
    result = factory()->NewScriptOrEvalFunctionLiteral(
        scope, body, function_state.expected_property_count(), parameter_count);
    result->set_suspend_count(function_state.suspend_count());
  }

  info->set_max_function_literal_id(GetLastFunctionLiteralId());

  // Make sure the target stack is empty.
  DCHECK_NULL(target_stack_);

  if (has_error()) return nullptr;

  RecordFunctionLiteralSourceRange(result);

  return result;
}

ZonePtrList<const AstRawString>* Parser::PrepareWrappedArguments(
    Isolate* isolate, ParseInfo* info, Zone* zone) {
  DCHECK(parsing_on_main_thread_);
  DCHECK_NOT_NULL(isolate);
  Handle<FixedArray> arguments(info->script()->wrapped_arguments(), isolate);
  int arguments_length = arguments->length();
  ZonePtrList<const AstRawString>* arguments_for_wrapped_function =
      new (zone) ZonePtrList<const AstRawString>(arguments_length, zone);
  for (int i = 0; i < arguments_length; i++) {
    const AstRawString* argument_string = ast_value_factory()->GetString(
        Handle<String>(String::cast(arguments->get(i)), isolate));
    arguments_for_wrapped_function->Add(argument_string, zone);
  }
  return arguments_for_wrapped_function;
}

void Parser::ParseWrapped(Isolate* isolate, ParseInfo* info,
                          ScopedPtrList<Statement>* body,
                          DeclarationScope* outer_scope, Zone* zone) {
  DCHECK_EQ(parsing_on_main_thread_, isolate != nullptr);
  DCHECK(info->is_wrapped_as_function());
  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  // Set function and block state for the outer eval scope.
  DCHECK(outer_scope->is_eval_scope());
  FunctionState function_state(&function_state_, &scope_, outer_scope);

  const AstRawString* function_name = nullptr;
  Scanner::Location location(0, 0);

  ZonePtrList<const AstRawString>* arguments_for_wrapped_function =
      PrepareWrappedArguments(isolate, info, zone);

  FunctionLiteral* function_literal = ParseFunctionLiteral(
      function_name, location, kSkipFunctionNameCheck, kNormalFunction,
      kNoSourcePosition, FunctionSyntaxKind::kWrapped, LanguageMode::kSloppy,
      arguments_for_wrapped_function);

  Statement* return_statement = factory()->NewReturnStatement(
      function_literal, kNoSourcePosition, kNoSourcePosition);
  body->Add(return_statement);
}

FunctionLiteral* Parser::ParseFunction(Isolate* isolate, ParseInfo* info,
                                       Handle<SharedFunctionInfo> shared_info) {
  // It's OK to use the Isolate & counters here, since this function is only
  // called in the main thread.
  DCHECK(parsing_on_main_thread_);
  RuntimeCallTimerScope runtime_timer(runtime_call_stats_,
                                      RuntimeCallCounterId::kParseFunction);
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.ParseFunction");
  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();

  DeserializeScopeChain(isolate, info, info->maybe_outer_scope_info(),
                        Scope::DeserializationMode::kIncludingVariables);
  DCHECK_EQ(factory()->zone(), info->zone());

  // Initialize parser state.
  Handle<String> name(shared_info->Name(), isolate);
  info->set_function_name(ast_value_factory()->GetString(name));
  scanner_.Initialize();

  FunctionLiteral* result;
  if (V8_UNLIKELY(shared_info->private_name_lookup_skips_outer_class() &&
                  original_scope_->is_class_scope())) {
    // If the function skips the outer class and the outer scope is a class, the
    // function is in heritage position. Otherwise the function scope's skip bit
    // will be correctly inherited from the outer scope.
    ClassScope::HeritageParsingScope heritage(original_scope_->AsClassScope());
    result = DoParseFunction(isolate, info, info->function_name());
  } else {
    result = DoParseFunction(isolate, info, info->function_name());
  }
  MaybeResetCharacterStream(info, result);
  MaybeProcessSourceRanges(info, result, stack_limit_);
  if (result != nullptr) {
    Handle<String> inferred_name(shared_info->inferred_name(), isolate);
    result->set_inferred_name(inferred_name);
  }

  if (V8_UNLIKELY(FLAG_log_function_events) && result != nullptr) {
    double ms = timer.Elapsed().InMillisecondsF();
    // We need to make sure that the debug-name is available.
    ast_value_factory()->Internalize(isolate);
    DeclarationScope* function_scope = result->scope();
    std::unique_ptr<char[]> function_name = result->GetDebugName();
    LOG(isolate,
        FunctionEvent("parse-function", info->script()->id(), ms,
                      function_scope->start_position(),
                      function_scope->end_position(), function_name.get(),
                      strlen(function_name.get())));
  }
  return result;
}

FunctionLiteral* Parser::DoParseFunction(Isolate* isolate, ParseInfo* info,
                                         const AstRawString* raw_name) {
  DCHECK_EQ(parsing_on_main_thread_, isolate != nullptr);
  DCHECK_NOT_NULL(raw_name);
  DCHECK_NULL(scope_);
  DCHECK_NULL(target_stack_);

  DCHECK(ast_value_factory());
  fni_.PushEnclosingName(raw_name);

  ResetFunctionLiteralId();
  DCHECK_LT(0, info->function_literal_id());
  SkipFunctionLiterals(info->function_literal_id() - 1);

  ParsingModeScope parsing_mode(this, PARSE_EAGERLY);

  // Place holder for the result.
  FunctionLiteral* result = nullptr;

  {
    // Parse the function literal.
    Scope* outer = original_scope_;
    DeclarationScope* outer_function = outer->GetClosureScope();
    DCHECK(outer);
    FunctionState function_state(&function_state_, &scope_, outer_function);
    BlockState block_state(&scope_, outer);
    DCHECK(is_sloppy(outer->language_mode()) ||
           is_strict(info->language_mode()));
    FunctionKind kind = info->function_kind();
    DCHECK_IMPLIES(
        IsConciseMethod(kind) || IsAccessorFunction(kind),
        info->function_syntax_kind() == FunctionSyntaxKind::kAccessorOrMethod);

    if (IsArrowFunction(kind)) {
      if (IsAsyncFunction(kind)) {
        DCHECK(!scanner()->HasLineTerminatorAfterNext());
        if (!Check(Token::ASYNC)) {
          CHECK(stack_overflow());
          return nullptr;
        }
        if (!(peek_any_identifier() || peek() == Token::LPAREN)) {
          CHECK(stack_overflow());
          return nullptr;
        }
      }

      // TODO(adamk): We should construct this scope from the ScopeInfo.
      DeclarationScope* scope = NewFunctionScope(kind);
      scope->set_has_checked_syntax(true);

      // This bit only needs to be explicitly set because we're
      // not passing the ScopeInfo to the Scope constructor.
      SetLanguageMode(scope, info->language_mode());

      scope->set_start_position(info->start_position());
      ParserFormalParameters formals(scope);
      {
        ParameterDeclarationParsingScope formals_scope(this);
        // Parsing patterns as variable reference expression creates
        // NewUnresolved references in current scope. Enter arrow function
        // scope for formal parameter parsing.
        BlockState block_state(&scope_, scope);
        if (Check(Token::LPAREN)) {
          // '(' StrictFormalParameters ')'
          ParseFormalParameterList(&formals);
          Expect(Token::RPAREN);
        } else {
          // BindingIdentifier
          ParameterParsingScope scope(impl(), &formals);
          ParseFormalParameter(&formals);
          DeclareFormalParameters(&formals);
        }
        formals.duplicate_loc = formals_scope.duplicate_location();
      }

      if (GetLastFunctionLiteralId() != info->function_literal_id() - 1) {
        if (has_error()) return nullptr;
        // If there were FunctionLiterals in the parameters, we need to
        // renumber them to shift down so the next function literal id for
        // the arrow function is the one requested.
        AstFunctionLiteralIdReindexer reindexer(
            stack_limit_,
            (info->function_literal_id() - 1) - GetLastFunctionLiteralId());
        for (auto p : formals.params) {
          if (p->pattern != nullptr) reindexer.Reindex(p->pattern);
          if (p->initializer() != nullptr) {
            reindexer.Reindex(p->initializer());
          }
        }
        ResetFunctionLiteralId();
        SkipFunctionLiterals(info->function_literal_id() - 1);
      }

      Expression* expression = ParseArrowFunctionLiteral(formals);
      // Scanning must end at the same position that was recorded
      // previously. If not, parsing has been interrupted due to a stack
      // overflow, at which point the partially parsed arrow function
      // concise body happens to be a valid expression. This is a problem
      // only for arrow functions with single expression bodies, since there
      // is no end token such as "}" for normal functions.
      if (scanner()->location().end_pos == info->end_position()) {
        // The pre-parser saw an arrow function here, so the full parser
        // must produce a FunctionLiteral.
        DCHECK(expression->IsFunctionLiteral());
        result = expression->AsFunctionLiteral();
      }
    } else if (IsDefaultConstructor(kind)) {
      DCHECK_EQ(scope(), outer);
      result = DefaultConstructor(raw_name, IsDerivedConstructor(kind),
                                  info->start_position(), info->end_position());
    } else {
      ZonePtrList<const AstRawString>* arguments_for_wrapped_function =
          info->is_wrapped_as_function()
              ? PrepareWrappedArguments(isolate, info, zone())
              : nullptr;
      result = ParseFunctionLiteral(
          raw_name, Scanner::Location::invalid(), kSkipFunctionNameCheck, kind,
          kNoSourcePosition, info->function_syntax_kind(),
          info->language_mode(), arguments_for_wrapped_function);
    }

    if (has_error()) return nullptr;
    result->set_requires_instance_members_initializer(
        info->requires_instance_members_initializer());
    if (info->is_oneshot_iife()) {
      result->mark_as_oneshot_iife();
    }
  }

  // Make sure the target stack is empty.
  DCHECK_NULL(target_stack_);
  DCHECK_IMPLIES(result,
                 info->function_literal_id() == result->function_literal_id());
  return result;
}

Statement* Parser::ParseModuleItem() {
  // ecma262/#prod-ModuleItem
  // ModuleItem :
  //    ImportDeclaration
  //    ExportDeclaration
  //    StatementListItem

  Token::Value next = peek();

  if (next == Token::EXPORT) {
    return ParseExportDeclaration();
  }

  if (next == Token::IMPORT) {
    // We must be careful not to parse a dynamic import expression as an import
    // declaration. Same for import.meta expressions.
    Token::Value peek_ahead = PeekAhead();
    if ((!allow_harmony_dynamic_import() || peek_ahead != Token::LPAREN) &&
        (!allow_harmony_import_meta() || peek_ahead != Token::PERIOD)) {
      ParseImportDeclaration();
      return factory()->EmptyStatement();
    }
  }

  return ParseStatementListItem();
}

void Parser::ParseModuleItemList(ScopedPtrList<Statement>* body) {
  // ecma262/#prod-Module
  // Module :
  //    ModuleBody?
  //
  // ecma262/#prod-ModuleItemList
  // ModuleBody :
  //    ModuleItem*

  DCHECK(scope()->is_module_scope());
  while (peek() != Token::EOS) {
    Statement* stat = ParseModuleItem();
    if (stat == nullptr) return;
    if (stat->IsEmptyStatement()) continue;
    body->Add(stat);
  }
}

const AstRawString* Parser::ParseModuleSpecifier() {
  // ModuleSpecifier :
  //    StringLiteral

  Expect(Token::STRING);
  return GetSymbol();
}

ZoneChunkList<Parser::ExportClauseData>* Parser::ParseExportClause(
    Scanner::Location* reserved_loc) {
  // ExportClause :
  //   '{' '}'
  //   '{' ExportsList '}'
  //   '{' ExportsList ',' '}'
  //
  // ExportsList :
  //   ExportSpecifier
  //   ExportsList ',' ExportSpecifier
  //
  // ExportSpecifier :
  //   IdentifierName
  //   IdentifierName 'as' IdentifierName
  ZoneChunkList<ExportClauseData>* export_data =
      new (zone()) ZoneChunkList<ExportClauseData>(zone());

  Expect(Token::LBRACE);

  Token::Value name_tok;
  while ((name_tok = peek()) != Token::RBRACE) {
    // Keep track of the first reserved word encountered in case our
    // caller needs to report an error.
    if (!reserved_loc->IsValid() &&
        !Token::IsValidIdentifier(name_tok, LanguageMode::kStrict, false,
                                  parsing_module_)) {
      *reserved_loc = scanner()->location();
    }
    const AstRawString* local_name = ParsePropertyName();
    const AstRawString* export_name = nullptr;
    Scanner::Location location = scanner()->location();
    if (CheckContextualKeyword(ast_value_factory()->as_string())) {
      export_name = ParsePropertyName();
      // Set the location to the whole "a as b" string, so that it makes sense
      // both for errors due to "a" and for errors due to "b".
      location.end_pos = scanner()->location().end_pos;
    }
    if (export_name == nullptr) {
      export_name = local_name;
    }
    export_data->push_back({export_name, local_name, location});
    if (peek() == Token::RBRACE) break;
    if (V8_UNLIKELY(!Check(Token::COMMA))) {
      ReportUnexpectedToken(Next());
      break;
    }
  }

  Expect(Token::RBRACE);
  return export_data;
}

ZonePtrList<const Parser::NamedImport>* Parser::ParseNamedImports(int pos) {
  // NamedImports :
  //   '{' '}'
  //   '{' ImportsList '}'
  //   '{' ImportsList ',' '}'
  //
  // ImportsList :
  //   ImportSpecifier
  //   ImportsList ',' ImportSpecifier
  //
  // ImportSpecifier :
  //   BindingIdentifier
  //   IdentifierName 'as' BindingIdentifier

  Expect(Token::LBRACE);

  auto result = new (zone()) ZonePtrList<const NamedImport>(1, zone());
  while (peek() != Token::RBRACE) {
    const AstRawString* import_name = ParsePropertyName();
    const AstRawString* local_name = import_name;
    Scanner::Location location = scanner()->location();
    // In the presence of 'as', the left-side of the 'as' can
    // be any IdentifierName. But without 'as', it must be a valid
    // BindingIdentifier.
    if (CheckContextualKeyword(ast_value_factory()->as_string())) {
      local_name = ParsePropertyName();
    }
    if (!Token::IsValidIdentifier(scanner()->current_token(),
                                  LanguageMode::kStrict, false,
                                  parsing_module_)) {
      ReportMessage(MessageTemplate::kUnexpectedReserved);
      return nullptr;
    } else if (IsEvalOrArguments(local_name)) {
      ReportMessage(MessageTemplate::kStrictEvalArguments);
      return nullptr;
    }

    DeclareUnboundVariable(local_name, VariableMode::kConst,
                           kNeedsInitialization, position());

    NamedImport* import =
        new (zone()) NamedImport(import_name, local_name, location);
    result->Add(import, zone());

    if (peek() == Token::RBRACE) break;
    Expect(Token::COMMA);
  }

  Expect(Token::RBRACE);
  return result;
}

void Parser::ParseImportDeclaration() {
  // ImportDeclaration :
  //   'import' ImportClause 'from' ModuleSpecifier ';'
  //   'import' ModuleSpecifier ';'
  //
  // ImportClause :
  //   ImportedDefaultBinding
  //   NameSpaceImport
  //   NamedImports
  //   ImportedDefaultBinding ',' NameSpaceImport
  //   ImportedDefaultBinding ',' NamedImports
  //
  // NameSpaceImport :
  //   '*' 'as' ImportedBinding

  int pos = peek_position();
  Expect(Token::IMPORT);

  Token::Value tok = peek();

  // 'import' ModuleSpecifier ';'
  if (tok == Token::STRING) {
    Scanner::Location specifier_loc = scanner()->peek_location();
    const AstRawString* module_specifier = ParseModuleSpecifier();
    ExpectSemicolon();
    module()->AddEmptyImport(module_specifier, specifier_loc);
    return;
  }

  // Parse ImportedDefaultBinding if present.
  const AstRawString* import_default_binding = nullptr;
  Scanner::Location import_default_binding_loc;
  if (tok != Token::MUL && tok != Token::LBRACE) {
    import_default_binding = ParseNonRestrictedIdentifier();
    import_default_binding_loc = scanner()->location();
    DeclareUnboundVariable(import_default_binding, VariableMode::kConst,
                           kNeedsInitialization, pos);
  }

  // Parse NameSpaceImport or NamedImports if present.
  const AstRawString* module_namespace_binding = nullptr;
  Scanner::Location module_namespace_binding_loc;
  const ZonePtrList<const NamedImport>* named_imports = nullptr;
  if (import_default_binding == nullptr || Check(Token::COMMA)) {
    switch (peek()) {
      case Token::MUL: {
        Consume(Token::MUL);
        ExpectContextualKeyword(ast_value_factory()->as_string());
        module_namespace_binding = ParseNonRestrictedIdentifier();
        module_namespace_binding_loc = scanner()->location();
        DeclareUnboundVariable(module_namespace_binding, VariableMode::kConst,
                               kCreatedInitialized, pos);
        break;
      }

      case Token::LBRACE:
        named_imports = ParseNamedImports(pos);
        break;

      default:
        ReportUnexpectedToken(scanner()->current_token());
        return;
    }
  }

  ExpectContextualKeyword(ast_value_factory()->from_string());
  Scanner::Location specifier_loc = scanner()->peek_location();
  const AstRawString* module_specifier = ParseModuleSpecifier();
  ExpectSemicolon();

  // Now that we have all the information, we can make the appropriate
  // declarations.

  // TODO(neis): Would prefer to call DeclareVariable for each case below rather
  // than above and in ParseNamedImports, but then a possible error message
  // would point to the wrong location.  Maybe have a DeclareAt version of
  // Declare that takes a location?

  if (module_namespace_binding != nullptr) {
    module()->AddStarImport(module_namespace_binding, module_specifier,
                            module_namespace_binding_loc, specifier_loc,
                            zone());
  }

  if (import_default_binding != nullptr) {
    module()->AddImport(ast_value_factory()->default_string(),
                        import_default_binding, module_specifier,
                        import_default_binding_loc, specifier_loc, zone());
  }

  if (named_imports != nullptr) {
    if (named_imports->length() == 0) {
      module()->AddEmptyImport(module_specifier, specifier_loc);
    } else {
      for (const NamedImport* import : *named_imports) {
        module()->AddImport(import->import_name, import->local_name,
                            module_specifier, import->location, specifier_loc,
                            zone());
      }
    }
  }
}

Statement* Parser::ParseExportDefault() {
  //  Supports the following productions, starting after the 'default' token:
  //    'export' 'default' HoistableDeclaration
  //    'export' 'default' ClassDeclaration
  //    'export' 'default' AssignmentExpression[In] ';'

  Expect(Token::DEFAULT);
  Scanner::Location default_loc = scanner()->location();

  ZonePtrList<const AstRawString> local_names(1, zone());
  Statement* result = nullptr;
  switch (peek()) {
    case Token::FUNCTION:
      result = ParseHoistableDeclaration(&local_names, true);
      break;

    case Token::CLASS:
      Consume(Token::CLASS);
      result = ParseClassDeclaration(&local_names, true);
      break;

    case Token::ASYNC:
      if (PeekAhead() == Token::FUNCTION &&
          !scanner()->HasLineTerminatorAfterNext()) {
        Consume(Token::ASYNC);
        result = ParseAsyncFunctionDeclaration(&local_names, true);
        break;
      }
      V8_FALLTHROUGH;

    default: {
      int pos = position();
      AcceptINScope scope(this, true);
      Expression* value = ParseAssignmentExpression();
      SetFunctionName(value, ast_value_factory()->default_string());

      const AstRawString* local_name =
          ast_value_factory()->dot_default_string();
      local_names.Add(local_name, zone());

      // It's fine to declare this as VariableMode::kConst because the user has
      // no way of writing to it.
      VariableProxy* proxy =
          DeclareBoundVariable(local_name, VariableMode::kConst, pos);
      proxy->var()->set_initializer_position(position());

      Assignment* assignment = factory()->NewAssignment(
          Token::INIT, proxy, value, kNoSourcePosition);
      result = IgnoreCompletion(
          factory()->NewExpressionStatement(assignment, kNoSourcePosition));

      ExpectSemicolon();
      break;
    }
  }

  if (result != nullptr) {
    DCHECK_EQ(local_names.length(), 1);
    module()->AddExport(local_names.first(),
                        ast_value_factory()->default_string(), default_loc,
                        zone());
  }

  return result;
}

const AstRawString* Parser::NextInternalNamespaceExportName() {
  const char* prefix = ".ns-export";
  std::string s(prefix);
  s.append(std::to_string(number_of_named_namespace_exports_++));
  return ast_value_factory()->GetOneByteString(s.c_str());
}

void Parser::ParseExportStar() {
  int pos = position();
  Consume(Token::MUL);

  if (!FLAG_harmony_namespace_exports ||
      !PeekContextualKeyword(ast_value_factory()->as_string())) {
    // 'export' '*' 'from' ModuleSpecifier ';'
    Scanner::Location loc = scanner()->location();
    ExpectContextualKeyword(ast_value_factory()->from_string());
    Scanner::Location specifier_loc = scanner()->peek_location();
    const AstRawString* module_specifier = ParseModuleSpecifier();
    ExpectSemicolon();
    module()->AddStarExport(module_specifier, loc, specifier_loc, zone());
    return;
  }
  if (!FLAG_harmony_namespace_exports) return;

  // 'export' '*' 'as' IdentifierName 'from' ModuleSpecifier ';'
  //
  // Desugaring:
  //   export * as x from "...";
  // ~>
  //   import * as .x from "..."; export {.x as x};

  ExpectContextualKeyword(ast_value_factory()->as_string());
  const AstRawString* export_name = ParsePropertyName();
  Scanner::Location export_name_loc = scanner()->location();
  const AstRawString* local_name = NextInternalNamespaceExportName();
  Scanner::Location local_name_loc = Scanner::Location::invalid();
  DeclareUnboundVariable(local_name, VariableMode::kConst, kCreatedInitialized,
                         pos);

  ExpectContextualKeyword(ast_value_factory()->from_string());
  Scanner::Location specifier_loc = scanner()->peek_location();
  const AstRawString* module_specifier = ParseModuleSpecifier();
  ExpectSemicolon();

  module()->AddStarImport(local_name, module_specifier, local_name_loc,
                          specifier_loc, zone());
  module()->AddExport(local_name, export_name, export_name_loc, zone());
}

Statement* Parser::ParseExportDeclaration() {
  // ExportDeclaration:
  //    'export' '*' 'from' ModuleSpecifier ';'
  //    'export' '*' 'as' IdentifierName 'from' ModuleSpecifier ';'
  //    'export' ExportClause ('from' ModuleSpecifier)? ';'
  //    'export' VariableStatement
  //    'export' Declaration
  //    'export' 'default' ... (handled in ParseExportDefault)

  Expect(Token::EXPORT);
  Statement* result = nullptr;
  ZonePtrList<const AstRawString> names(1, zone());
  Scanner::Location loc = scanner()->peek_location();
  switch (peek()) {
    case Token::DEFAULT:
      return ParseExportDefault();

    case Token::MUL:
      ParseExportStar();
      return factory()->EmptyStatement();

    case Token::LBRACE: {
      // There are two cases here:
      //
      // 'export' ExportClause ';'
      // and
      // 'export' ExportClause FromClause ';'
      //
      // In the first case, the exported identifiers in ExportClause must
      // not be reserved words, while in the latter they may be. We
      // pass in a location that gets filled with the first reserved word
      // encountered, and then throw a SyntaxError if we are in the
      // non-FromClause case.
      Scanner::Location reserved_loc = Scanner::Location::invalid();
      ZoneChunkList<ExportClauseData>* export_data =
          ParseExportClause(&reserved_loc);
      const AstRawString* module_specifier = nullptr;
      Scanner::Location specifier_loc;
      if (CheckContextualKeyword(ast_value_factory()->from_string())) {
        specifier_loc = scanner()->peek_location();
        module_specifier = ParseModuleSpecifier();
      } else if (reserved_loc.IsValid()) {
        // No FromClause, so reserved words are invalid in ExportClause.
        ReportMessageAt(reserved_loc, MessageTemplate::kUnexpectedReserved);
        return nullptr;
      }
      ExpectSemicolon();
      if (module_specifier == nullptr) {
        for (const ExportClauseData& data : *export_data) {
          module()->AddExport(data.local_name, data.export_name, data.location,
                              zone());
        }
      } else if (export_data->is_empty()) {
        module()->AddEmptyImport(module_specifier, specifier_loc);
      } else {
        for (const ExportClauseData& data : *export_data) {
          module()->AddExport(data.local_name, data.export_name,
                              module_specifier, data.location, specifier_loc,
                              zone());
        }
      }
      return factory()->EmptyStatement();
    }

    case Token::FUNCTION:
      result = ParseHoistableDeclaration(&names, false);
      break;

    case Token::CLASS:
      Consume(Token::CLASS);
      result = ParseClassDeclaration(&names, false);
      break;

    case Token::VAR:
    case Token::LET:
    case Token::CONST:
      result = ParseVariableStatement(kStatementListItem, &names);
      break;

    case Token::ASYNC:
      Consume(Token::ASYNC);
      if (peek() == Token::FUNCTION &&
          !scanner()->HasLineTerminatorBeforeNext()) {
        result = ParseAsyncFunctionDeclaration(&names, false);
        break;
      }
      V8_FALLTHROUGH;

    default:
      ReportUnexpectedToken(scanner()->current_token());
      return nullptr;
  }
  loc.end_pos = scanner()->location().end_pos;

  SourceTextModuleDescriptor* descriptor = module();
  for (const AstRawString* name : names) {
    descriptor->AddExport(name, name, loc, zone());
  }

  return result;
}

void Parser::DeclareUnboundVariable(const AstRawString* name, VariableMode mode,
                                    InitializationFlag init, int pos) {
  bool was_added;
  Variable* var = DeclareVariable(name, NORMAL_VARIABLE, mode, init, scope(),
                                  &was_added, pos, end_position());
  // The variable will be added to the declarations list, but since we are not
  // binding it to anything, we can simply ignore it here.
  USE(var);
}

VariableProxy* Parser::DeclareBoundVariable(const AstRawString* name,
                                            VariableMode mode, int pos) {
  DCHECK_NOT_NULL(name);
  VariableProxy* proxy =
      factory()->NewVariableProxy(name, NORMAL_VARIABLE, position());
  bool was_added;
  Variable* var = DeclareVariable(name, NORMAL_VARIABLE, mode,
                                  Variable::DefaultInitializationFlag(mode),
                                  scope(), &was_added, pos, end_position());
  proxy->BindTo(var);
  return proxy;
}

void Parser::DeclareAndBindVariable(VariableProxy* proxy, VariableKind kind,
                                    VariableMode mode, Scope* scope,
                                    bool* was_added, int initializer_position) {
  Variable* var = DeclareVariable(
      proxy->raw_name(), kind, mode, Variable::DefaultInitializationFlag(mode),
      scope, was_added, proxy->position(), kNoSourcePosition);
  var->set_initializer_position(initializer_position);
  proxy->BindTo(var);
}

Variable* Parser::DeclareVariable(const AstRawString* name, VariableKind kind,
                                  VariableMode mode, InitializationFlag init,
                                  Scope* scope, bool* was_added, int begin,
                                  int end) {
  Declaration* declaration;
  if (mode == VariableMode::kVar && !scope->is_declaration_scope()) {
    DCHECK(scope->is_block_scope() || scope->is_with_scope());
    declaration = factory()->NewNestedVariableDeclaration(scope, begin);
  } else {
    declaration = factory()->NewVariableDeclaration(begin);
  }
  Declare(declaration, name, kind, mode, init, scope, was_added, begin, end);
  return declaration->var();
}

void Parser::Declare(Declaration* declaration, const AstRawString* name,
                     VariableKind variable_kind, VariableMode mode,
                     InitializationFlag init, Scope* scope, bool* was_added,
                     int var_begin_pos, int var_end_pos) {
  bool local_ok = true;
  bool sloppy_mode_block_scope_function_redefinition = false;
  scope->DeclareVariable(
      declaration, name, var_begin_pos, mode, variable_kind, init, was_added,
      &sloppy_mode_block_scope_function_redefinition, &local_ok);
  if (!local_ok) {
    // If we only have the start position of a proxy, we can't highlight the
    // whole variable name.  Pretend its length is 1 so that we highlight at
    // least the first character.
    Scanner::Location loc(var_begin_pos, var_end_pos != kNoSourcePosition
                                             ? var_end_pos
                                             : var_begin_pos + 1);
    if (variable_kind == PARAMETER_VARIABLE) {
      ReportMessageAt(loc, MessageTemplate::kParamDupe);
    } else {
      ReportMessageAt(loc, MessageTemplate::kVarRedeclaration,
                      declaration->var()->raw_name());
    }
  } else if (sloppy_mode_block_scope_function_redefinition) {
    ++use_counts_[v8::Isolate::kSloppyModeBlockScopedFunctionRedefinition];
  }
}

Statement* Parser::BuildInitializationBlock(
    DeclarationParsingResult* parsing_result) {
  ScopedPtrList<Statement> statements(pointer_buffer());
  for (const auto& declaration : parsing_result->declarations) {
    if (!declaration.initializer) continue;
    InitializeVariables(&statements, parsing_result->descriptor.kind,
                        &declaration);
  }
  return factory()->NewBlock(true, statements);
}

Statement* Parser::DeclareFunction(const AstRawString* variable_name,
                                   FunctionLiteral* function, VariableMode mode,
                                   VariableKind kind, int beg_pos, int end_pos,
                                   ZonePtrList<const AstRawString>* names) {
  Declaration* declaration =
      factory()->NewFunctionDeclaration(function, beg_pos);
  bool was_added;
  Declare(declaration, variable_name, kind, mode, kCreatedInitialized, scope(),
          &was_added, beg_pos);
  if (info()->coverage_enabled()) {
    // Force the function to be allocated when collecting source coverage, so
    // that even dead functions get source coverage data.
    declaration->var()->set_is_used();
  }
  if (names) names->Add(variable_name, zone());
  if (kind == SLOPPY_BLOCK_FUNCTION_VARIABLE) {
    Token::Value init = loop_nesting_depth() > 0 ? Token::ASSIGN : Token::INIT;
    SloppyBlockFunctionStatement* statement =
        factory()->NewSloppyBlockFunctionStatement(end_pos, declaration->var(),
                                                   init);
    GetDeclarationScope()->DeclareSloppyBlockFunction(statement);
    return statement;
  }
  return factory()->EmptyStatement();
}

Statement* Parser::DeclareClass(const AstRawString* variable_name,
                                Expression* value,
                                ZonePtrList<const AstRawString>* names,
                                int class_token_pos, int end_pos) {
  VariableProxy* proxy =
      DeclareBoundVariable(variable_name, VariableMode::kLet, class_token_pos);
  proxy->var()->set_initializer_position(end_pos);
  if (names) names->Add(variable_name, zone());

  Assignment* assignment =
      factory()->NewAssignment(Token::INIT, proxy, value, class_token_pos);
  return IgnoreCompletion(
      factory()->NewExpressionStatement(assignment, kNoSourcePosition));
}

Statement* Parser::DeclareNative(const AstRawString* name, int pos) {
  // Make sure that the function containing the native declaration
  // isn't lazily compiled. The extension structures are only
  // accessible while parsing the first time not when reparsing
  // because of lazy compilation.
  GetClosureScope()->ForceEagerCompilation();

  // TODO(1240846): It's weird that native function declarations are
  // introduced dynamically when we meet their declarations, whereas
  // other functions are set up when entering the surrounding scope.
  VariableProxy* proxy = DeclareBoundVariable(name, VariableMode::kVar, pos);
  NativeFunctionLiteral* lit =
      factory()->NewNativeFunctionLiteral(name, extension_, kNoSourcePosition);
  return factory()->NewExpressionStatement(
      factory()->NewAssignment(Token::INIT, proxy, lit, kNoSourcePosition),
      pos);
}

void Parser::DeclareLabel(ZonePtrList<const AstRawString>** labels,
                          ZonePtrList<const AstRawString>** own_labels,
                          const AstRawString* label) {
  // TODO(1240780): We don't check for redeclaration of labels during preparsing
  // since keeping track of the set of active labels requires nontrivial changes
  // to the way scopes are structured.  However, these are probably changes we
  // want to make later anyway so we should go back and fix this then.
  if (ContainsLabel(*labels, label) || TargetStackContainsLabel(label)) {
    ReportMessage(MessageTemplate::kLabelRedeclaration, label);
    return;
  }

  // Add {label} to both {labels} and {own_labels}.
  if (*labels == nullptr) {
    DCHECK_NULL(*own_labels);
    *labels = new (zone()) ZonePtrList<const AstRawString>(1, zone());
    *own_labels = new (zone()) ZonePtrList<const AstRawString>(1, zone());
  } else {
    if (*own_labels == nullptr) {
      *own_labels = new (zone()) ZonePtrList<const AstRawString>(1, zone());
    }
  }
  (*labels)->Add(label, zone());
  (*own_labels)->Add(label, zone());
}

bool Parser::ContainsLabel(ZonePtrList<const AstRawString>* labels,
                           const AstRawString* label) {
  DCHECK_NOT_NULL(label);
  if (labels != nullptr) {
    for (int i = labels->length(); i-- > 0;) {
      if (labels->at(i) == label) return true;
    }
  }
  return false;
}

Block* Parser::IgnoreCompletion(Statement* statement) {
  Block* block = factory()->NewBlock(1, true);
  block->statements()->Add(statement, zone());
  return block;
}

Expression* Parser::RewriteReturn(Expression* return_value, int pos) {
  if (IsDerivedConstructor(function_state_->kind())) {
    // For subclass constructors we need to return this in case of undefined;
    // other primitive values trigger an exception in the ConstructStub.
    //
    //   return expr;
    //
    // Is rewritten as:
    //
    //   return (temp = expr) === undefined ? this : temp;

    // temp = expr
    Variable* temp = NewTemporary(ast_value_factory()->empty_string());
    Assignment* assign = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(temp), return_value, pos);

    // temp === undefined
    Expression* is_undefined = factory()->NewCompareOperation(
        Token::EQ_STRICT, assign,
        factory()->NewUndefinedLiteral(kNoSourcePosition), pos);

    // is_undefined ? this : temp
    // We don't need to call UseThis() since it's guaranteed to be called
    // for derived constructors after parsing the constructor in
    // ParseFunctionBody.
    return_value =
        factory()->NewConditional(is_undefined, factory()->ThisExpression(),
                                  factory()->NewVariableProxy(temp), pos);
  }
  return return_value;
}

Statement* Parser::RewriteSwitchStatement(SwitchStatement* switch_statement,
                                          Scope* scope) {
  // In order to get the CaseClauses to execute in their own lexical scope,
  // but without requiring downstream code to have special scope handling
  // code for switch statements, desugar into blocks as follows:
  // {  // To group the statements--harmless to evaluate Expression in scope
  //   .tag_variable = Expression;
  //   {  // To give CaseClauses a scope
  //     switch (.tag_variable) { CaseClause* }
  //   }
  // }
  DCHECK_NOT_NULL(scope);
  DCHECK(scope->is_block_scope());
  DCHECK_GE(switch_statement->position(), scope->start_position());
  DCHECK_LT(switch_statement->position(), scope->end_position());

  Block* switch_block = factory()->NewBlock(2, false);

  Expression* tag = switch_statement->tag();
  Variable* tag_variable =
      NewTemporary(ast_value_factory()->dot_switch_tag_string());
  Assignment* tag_assign = factory()->NewAssignment(
      Token::ASSIGN, factory()->NewVariableProxy(tag_variable), tag,
      tag->position());
  // Wrap with IgnoreCompletion so the tag isn't returned as the completion
  // value, in case the switch statements don't have a value.
  Statement* tag_statement = IgnoreCompletion(
      factory()->NewExpressionStatement(tag_assign, kNoSourcePosition));
  switch_block->statements()->Add(tag_statement, zone());

  switch_statement->set_tag(factory()->NewVariableProxy(tag_variable));
  Block* cases_block = factory()->NewBlock(1, false);
  cases_block->statements()->Add(switch_statement, zone());
  cases_block->set_scope(scope);
  switch_block->statements()->Add(cases_block, zone());
  return switch_block;
}

void Parser::InitializeVariables(
    ScopedPtrList<Statement>* statements, VariableKind kind,
    const DeclarationParsingResult::Declaration* declaration) {
  if (has_error()) return;

  DCHECK_NOT_NULL(declaration->initializer);

  int pos = declaration->value_beg_pos;
  if (pos == kNoSourcePosition) {
    pos = declaration->initializer->position();
  }
  Assignment* assignment = factory()->NewAssignment(
      Token::INIT, declaration->pattern, declaration->initializer, pos);
  statements->Add(factory()->NewExpressionStatement(assignment, pos));
}

Block* Parser::RewriteCatchPattern(CatchInfo* catch_info) {
  DCHECK_NOT_NULL(catch_info->pattern);

  DeclarationParsingResult::Declaration decl(
      catch_info->pattern, factory()->NewVariableProxy(catch_info->variable));

  ScopedPtrList<Statement> init_statements(pointer_buffer());
  InitializeVariables(&init_statements, NORMAL_VARIABLE, &decl);
  return factory()->NewBlock(true, init_statements);
}

void Parser::ReportVarRedeclarationIn(const AstRawString* name, Scope* scope) {
  for (Declaration* decl : *scope->declarations()) {
    if (decl->var()->raw_name() == name) {
      int position = decl->position();
      Scanner::Location location =
          position == kNoSourcePosition
              ? Scanner::Location::invalid()
              : Scanner::Location(position, position + name->length());
      ReportMessageAt(location, MessageTemplate::kVarRedeclaration, name);
      return;
    }
  }
  UNREACHABLE();
}

Statement* Parser::RewriteTryStatement(Block* try_block, Block* catch_block,
                                       const SourceRange& catch_range,
                                       Block* finally_block,
                                       const SourceRange& finally_range,
                                       const CatchInfo& catch_info, int pos) {
  // Simplify the AST nodes by converting:
  //   'try B0 catch B1 finally B2'
  // to:
  //   'try { try B0 catch B1 } finally B2'

  if (catch_block != nullptr && finally_block != nullptr) {
    // If we have both, create an inner try/catch.
    TryCatchStatement* statement;
    statement = factory()->NewTryCatchStatement(try_block, catch_info.scope,
                                                catch_block, kNoSourcePosition);
    RecordTryCatchStatementSourceRange(statement, catch_range);

    try_block = factory()->NewBlock(1, false);
    try_block->statements()->Add(statement, zone());
    catch_block = nullptr;  // Clear to indicate it's been handled.
  }

  if (catch_block != nullptr) {
    DCHECK_NULL(finally_block);
    TryCatchStatement* stmt = factory()->NewTryCatchStatement(
        try_block, catch_info.scope, catch_block, pos);
    RecordTryCatchStatementSourceRange(stmt, catch_range);
    return stmt;
  } else {
    DCHECK_NOT_NULL(finally_block);
    TryFinallyStatement* stmt =
        factory()->NewTryFinallyStatement(try_block, finally_block, pos);
    RecordTryFinallyStatementSourceRange(stmt, finally_range);
    return stmt;
  }
}

void Parser::ParseAndRewriteGeneratorFunctionBody(
    int pos, FunctionKind kind, ScopedPtrList<Statement>* body) {
  // For ES6 Generators, we just prepend the initial yield.
  Expression* initial_yield = BuildInitialYield(pos, kind);
  body->Add(
      factory()->NewExpressionStatement(initial_yield, kNoSourcePosition));
  ParseStatementList(body, Token::RBRACE);
}

void Parser::ParseAndRewriteAsyncGeneratorFunctionBody(
    int pos, FunctionKind kind, ScopedPtrList<Statement>* body) {
  // For ES2017 Async Generators, we produce:
  //
  // try {
  //   InitialYield;
  //   ...body...;
  //   // fall through to the implicit return after the try-finally
  // } catch (.catch) {
  //   %AsyncGeneratorReject(generator, .catch);
  // } finally {
  //   %_GeneratorClose(generator);
  // }
  //
  // - InitialYield yields the actual generator object.
  // - Any return statement inside the body will have its argument wrapped
  //   in an iterator result object with a "done" property set to `true`.
  // - If the generator terminates for whatever reason, we must close it.
  //   Hence the finally clause.
  // - BytecodeGenerator performs special handling for ReturnStatements in
  //   async generator functions, resolving the appropriate Promise with an
  //   "done" iterator result object containing a Promise-unwrapped value.
  DCHECK(IsAsyncGeneratorFunction(kind));

  Block* try_block;
  {
    ScopedPtrList<Statement> statements(pointer_buffer());
    Expression* initial_yield = BuildInitialYield(pos, kind);
    statements.Add(
        factory()->NewExpressionStatement(initial_yield, kNoSourcePosition));
    ParseStatementList(&statements, Token::RBRACE);

    // Don't create iterator result for async generators, as the resume methods
    // will create it.
    try_block = factory()->NewBlock(false, statements);
  }

  // For AsyncGenerators, a top-level catch block will reject the Promise.
  Scope* catch_scope = NewHiddenCatchScope();

  Block* catch_block;
  {
    ScopedPtrList<Expression> reject_args(pointer_buffer());
    reject_args.Add(factory()->NewVariableProxy(
        function_state_->scope()->generator_object_var()));
    reject_args.Add(factory()->NewVariableProxy(catch_scope->catch_variable()));

    Expression* reject_call = factory()->NewCallRuntime(
        Runtime::kInlineAsyncGeneratorReject, reject_args, kNoSourcePosition);
    catch_block = IgnoreCompletion(
        factory()->NewReturnStatement(reject_call, kNoSourcePosition));
  }

  {
    ScopedPtrList<Statement> statements(pointer_buffer());
    TryStatement* try_catch = factory()->NewTryCatchStatementForAsyncAwait(
        try_block, catch_scope, catch_block, kNoSourcePosition);
    statements.Add(try_catch);
    try_block = factory()->NewBlock(false, statements);
  }

  Expression* close_call;
  {
    ScopedPtrList<Expression> close_args(pointer_buffer());
    VariableProxy* call_proxy = factory()->NewVariableProxy(
        function_state_->scope()->generator_object_var());
    close_args.Add(call_proxy);
    close_call = factory()->NewCallRuntime(Runtime::kInlineGeneratorClose,
                                           close_args, kNoSourcePosition);
  }

  Block* finally_block;
  {
    ScopedPtrList<Statement> statements(pointer_buffer());
    statements.Add(
        factory()->NewExpressionStatement(close_call, kNoSourcePosition));
    finally_block = factory()->NewBlock(false, statements);
  }

  body->Add(factory()->NewTryFinallyStatement(try_block, finally_block,
                                              kNoSourcePosition));
}

void Parser::DeclareFunctionNameVar(const AstRawString* function_name,
                                    FunctionSyntaxKind function_syntax_kind,
                                    DeclarationScope* function_scope) {
  if (function_syntax_kind == FunctionSyntaxKind::kNamedExpression &&
      function_scope->LookupLocal(function_name) == nullptr) {
    DCHECK_EQ(function_scope, scope());
    function_scope->DeclareFunctionVar(function_name);
  }
}

// Special case for legacy for
//
//    for (var x = initializer in enumerable) body
//
// An initialization block of the form
//
//    {
//      x = initializer;
//    }
//
// is returned in this case.  It has reserved space for two statements,
// so that (later on during parsing), the equivalent of
//
//   for (x in enumerable) body
//
// is added as a second statement to it.
Block* Parser::RewriteForVarInLegacy(const ForInfo& for_info) {
  const DeclarationParsingResult::Declaration& decl =
      for_info.parsing_result.declarations[0];
  if (!IsLexicalVariableMode(for_info.parsing_result.descriptor.mode) &&
      decl.initializer != nullptr && decl.pattern->IsVariableProxy()) {
    ++use_counts_[v8::Isolate::kForInInitializer];
    const AstRawString* name = decl.pattern->AsVariableProxy()->raw_name();
    VariableProxy* single_var = NewUnresolved(name);
    Block* init_block = factory()->NewBlock(2, true);
    init_block->statements()->Add(
        factory()->NewExpressionStatement(
            factory()->NewAssignment(Token::ASSIGN, single_var,
                                     decl.initializer, decl.value_beg_pos),
            kNoSourcePosition),
        zone());
    return init_block;
  }
  return nullptr;
}

// Rewrite a for-in/of statement of the form
//
//   for (let/const/var x in/of e) b
//
// into
//
//   {
//     var temp;
//     for (temp in/of e) {
//       let/const/var x = temp;
//       b;
//     }
//     let x;  // for TDZ
//   }
void Parser::DesugarBindingInForEachStatement(ForInfo* for_info,
                                              Block** body_block,
                                              Expression** each_variable) {
  DCHECK_EQ(1, for_info->parsing_result.declarations.size());
  DeclarationParsingResult::Declaration& decl =
      for_info->parsing_result.declarations[0];
  Variable* temp = NewTemporary(ast_value_factory()->dot_for_string());
  ScopedPtrList<Statement> each_initialization_statements(pointer_buffer());
  DCHECK_IMPLIES(!has_error(), decl.pattern != nullptr);
  decl.initializer = factory()->NewVariableProxy(temp, for_info->position);
  InitializeVariables(&each_initialization_statements, NORMAL_VARIABLE, &decl);

  *body_block = factory()->NewBlock(3, false);
  (*body_block)
      ->statements()
      ->Add(factory()->NewBlock(true, each_initialization_statements), zone());
  *each_variable = factory()->NewVariableProxy(temp, for_info->position);
}

// Create a TDZ for any lexically-bound names in for in/of statements.
Block* Parser::CreateForEachStatementTDZ(Block* init_block,
                                         const ForInfo& for_info) {
  if (IsLexicalVariableMode(for_info.parsing_result.descriptor.mode)) {
    DCHECK_NULL(init_block);

    init_block = factory()->NewBlock(1, false);

    for (const AstRawString* bound_name : for_info.bound_names) {
      // TODO(adamk): This needs to be some sort of special
      // INTERNAL variable that's invisible to the debugger
      // but visible to everything else.
      VariableProxy* tdz_proxy = DeclareBoundVariable(
          bound_name, VariableMode::kLet, kNoSourcePosition);
      tdz_proxy->var()->set_initializer_position(position());
    }
  }
  return init_block;
}

Statement* Parser::DesugarLexicalBindingsInForStatement(
    ForStatement* loop, Statement* init, Expression* cond, Statement* next,
    Statement* body, Scope* inner_scope, const ForInfo& for_info) {
  // ES6 13.7.4.8 specifies that on each loop iteration the let variables are
  // copied into a new environment.  Moreover, the "next" statement must be
  // evaluated not in the environment of the just completed iteration but in
  // that of the upcoming one.  We achieve this with the following desugaring.
  // Extra care is needed to preserve the completion value of the original loop.
  //
  // We are given a for statement of the form
  //
  //  labels: for (let/const x = i; cond; next) body
  //
  // and rewrite it as follows.  Here we write {{ ... }} for init-blocks, ie.,
  // blocks whose ignore_completion_value_ flag is set.
  //
  //  {
  //    let/const x = i;
  //    temp_x = x;
  //    first = 1;
  //    undefined;
  //    outer: for (;;) {
  //      let/const x = temp_x;
  //      {{ if (first == 1) {
  //           first = 0;
  //         } else {
  //           next;
  //         }
  //         flag = 1;
  //         if (!cond) break;
  //      }}
  //      labels: for (; flag == 1; flag = 0, temp_x = x) {
  //        body
  //      }
  //      {{ if (flag == 1)  // Body used break.
  //           break;
  //      }}
  //    }
  //  }

  DCHECK_GT(for_info.bound_names.length(), 0);
  ScopedPtrList<Variable> temps(pointer_buffer());

  Block* outer_block =
      factory()->NewBlock(for_info.bound_names.length() + 4, false);

  // Add statement: let/const x = i.
  outer_block->statements()->Add(init, zone());

  const AstRawString* temp_name = ast_value_factory()->dot_for_string();

  // For each lexical variable x:
  //   make statement: temp_x = x.
  for (const AstRawString* bound_name : for_info.bound_names) {
    VariableProxy* proxy = NewUnresolved(bound_name);
    Variable* temp = NewTemporary(temp_name);
    VariableProxy* temp_proxy = factory()->NewVariableProxy(temp);
    Assignment* assignment = factory()->NewAssignment(Token::ASSIGN, temp_proxy,
                                                      proxy, kNoSourcePosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, kNoSourcePosition);
    outer_block->statements()->Add(assignment_statement, zone());
    temps.Add(temp);
  }

  Variable* first = nullptr;
  // Make statement: first = 1.
  if (next) {
    first = NewTemporary(temp_name);
    VariableProxy* first_proxy = factory()->NewVariableProxy(first);
    Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, first_proxy, const1, kNoSourcePosition);
    Statement* assignment_statement =
        factory()->NewExpressionStatement(assignment, kNoSourcePosition);
    outer_block->statements()->Add(assignment_statement, zone());
  }

  // make statement: undefined;
  outer_block->statements()->Add(
      factory()->NewExpressionStatement(
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition),
      zone());

  // Make statement: outer: for (;;)
  // Note that we don't actually create the label, or set this loop up as an
  // explicit break target, instead handing it directly to those nodes that
  // need to know about it. This should be safe because we don't run any code
  // in this function that looks up break targets.
  ForStatement* outer_loop =
      factory()->NewForStatement(nullptr, nullptr, kNoSourcePosition);
  outer_block->statements()->Add(outer_loop, zone());
  outer_block->set_scope(scope());

  Block* inner_block = factory()->NewBlock(3, false);
  {
    BlockState block_state(&scope_, inner_scope);

    Block* ignore_completion_block =
        factory()->NewBlock(for_info.bound_names.length() + 3, true);
    ScopedPtrList<Variable> inner_vars(pointer_buffer());
    // For each let variable x:
    //    make statement: let/const x = temp_x.
    for (int i = 0; i < for_info.bound_names.length(); i++) {
      VariableProxy* proxy = DeclareBoundVariable(
          for_info.bound_names[i], for_info.parsing_result.descriptor.mode,
          kNoSourcePosition);
      inner_vars.Add(proxy->var());
      VariableProxy* temp_proxy = factory()->NewVariableProxy(temps.at(i));
      Assignment* assignment = factory()->NewAssignment(
          Token::INIT, proxy, temp_proxy, kNoSourcePosition);
      Statement* assignment_statement =
          factory()->NewExpressionStatement(assignment, kNoSourcePosition);
      int declaration_pos = for_info.parsing_result.descriptor.declaration_pos;
      DCHECK_NE(declaration_pos, kNoSourcePosition);
      proxy->var()->set_initializer_position(declaration_pos);
      ignore_completion_block->statements()->Add(assignment_statement, zone());
    }

    // Make statement: if (first == 1) { first = 0; } else { next; }
    if (next) {
      DCHECK(first);
      Expression* compare = nullptr;
      // Make compare expression: first == 1.
      {
        Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
        VariableProxy* first_proxy = factory()->NewVariableProxy(first);
        compare = factory()->NewCompareOperation(Token::EQ, first_proxy, const1,
                                                 kNoSourcePosition);
      }
      Statement* clear_first = nullptr;
      // Make statement: first = 0.
      {
        VariableProxy* first_proxy = factory()->NewVariableProxy(first);
        Expression* const0 = factory()->NewSmiLiteral(0, kNoSourcePosition);
        Assignment* assignment = factory()->NewAssignment(
            Token::ASSIGN, first_proxy, const0, kNoSourcePosition);
        clear_first =
            factory()->NewExpressionStatement(assignment, kNoSourcePosition);
      }
      Statement* clear_first_or_next = factory()->NewIfStatement(
          compare, clear_first, next, kNoSourcePosition);
      ignore_completion_block->statements()->Add(clear_first_or_next, zone());
    }

    Variable* flag = NewTemporary(temp_name);
    // Make statement: flag = 1.
    {
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
      Assignment* assignment = factory()->NewAssignment(
          Token::ASSIGN, flag_proxy, const1, kNoSourcePosition);
      Statement* assignment_statement =
          factory()->NewExpressionStatement(assignment, kNoSourcePosition);
      ignore_completion_block->statements()->Add(assignment_statement, zone());
    }

    // Make statement: if (!cond) break.
    if (cond) {
      Statement* stop =
          factory()->NewBreakStatement(outer_loop, kNoSourcePosition);
      Statement* noop = factory()->EmptyStatement();
      ignore_completion_block->statements()->Add(
          factory()->NewIfStatement(cond, noop, stop, cond->position()),
          zone());
    }

    inner_block->statements()->Add(ignore_completion_block, zone());
    // Make cond expression for main loop: flag == 1.
    Expression* flag_cond = nullptr;
    {
      Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
      VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
      flag_cond = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                                 kNoSourcePosition);
    }

    // Create chain of expressions "flag = 0, temp_x = x, ..."
    Statement* compound_next_statement = nullptr;
    {
      Expression* compound_next = nullptr;
      // Make expression: flag = 0.
      {
        VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
        Expression* const0 = factory()->NewSmiLiteral(0, kNoSourcePosition);
        compound_next = factory()->NewAssignment(Token::ASSIGN, flag_proxy,
                                                 const0, kNoSourcePosition);
      }

      // Make the comma-separated list of temp_x = x assignments.
      int inner_var_proxy_pos = scanner()->location().beg_pos;
      for (int i = 0; i < for_info.bound_names.length(); i++) {
        VariableProxy* temp_proxy = factory()->NewVariableProxy(temps.at(i));
        VariableProxy* proxy =
            factory()->NewVariableProxy(inner_vars.at(i), inner_var_proxy_pos);
        Assignment* assignment = factory()->NewAssignment(
            Token::ASSIGN, temp_proxy, proxy, kNoSourcePosition);
        compound_next = factory()->NewBinaryOperation(
            Token::COMMA, compound_next, assignment, kNoSourcePosition);
      }

      compound_next_statement =
          factory()->NewExpressionStatement(compound_next, kNoSourcePosition);
    }

    // Make statement: labels: for (; flag == 1; flag = 0, temp_x = x)
    // Note that we re-use the original loop node, which retains its labels
    // and ensures that any break or continue statements in body point to
    // the right place.
    loop->Initialize(nullptr, flag_cond, compound_next_statement, body);
    inner_block->statements()->Add(loop, zone());

    // Make statement: {{if (flag == 1) break;}}
    {
      Expression* compare = nullptr;
      // Make compare expresion: flag == 1.
      {
        Expression* const1 = factory()->NewSmiLiteral(1, kNoSourcePosition);
        VariableProxy* flag_proxy = factory()->NewVariableProxy(flag);
        compare = factory()->NewCompareOperation(Token::EQ, flag_proxy, const1,
                                                 kNoSourcePosition);
      }
      Statement* stop =
          factory()->NewBreakStatement(outer_loop, kNoSourcePosition);
      Statement* empty = factory()->EmptyStatement();
      Statement* if_flag_break =
          factory()->NewIfStatement(compare, stop, empty, kNoSourcePosition);
      inner_block->statements()->Add(IgnoreCompletion(if_flag_break), zone());
    }

    inner_block->set_scope(inner_scope);
  }

  outer_loop->Initialize(nullptr, nullptr, nullptr, inner_block);

  return outer_block;
}

void ParserFormalParameters::ValidateDuplicate(Parser* parser) const {
  if (has_duplicate()) {
    parser->ReportMessageAt(duplicate_loc, MessageTemplate::kParamDupe);
  }
}
void ParserFormalParameters::ValidateStrictMode(Parser* parser) const {
  if (strict_error_loc.IsValid()) {
    parser->ReportMessageAt(strict_error_loc, strict_error_message);
  }
}

void Parser::AddArrowFunctionFormalParameters(
    ParserFormalParameters* parameters, Expression* expr, int end_pos) {
  // ArrowFunctionFormals ::
  //    Nary(Token::COMMA, VariableProxy*, Tail)
  //    Binary(Token::COMMA, NonTailArrowFunctionFormals, Tail)
  //    Tail
  // NonTailArrowFunctionFormals ::
  //    Binary(Token::COMMA, NonTailArrowFunctionFormals, VariableProxy)
  //    VariableProxy
  // Tail ::
  //    VariableProxy
  //    Spread(VariableProxy)
  //
  // We need to visit the parameters in left-to-right order
  //

  // For the Nary case, we simply visit the parameters in a loop.
  if (expr->IsNaryOperation()) {
    NaryOperation* nary = expr->AsNaryOperation();
    // The classifier has already run, so we know that the expression is a valid
    // arrow function formals production.
    DCHECK_EQ(nary->op(), Token::COMMA);
    // Each op position is the end position of the *previous* expr, with the
    // second (i.e. first "subsequent") op position being the end position of
    // the first child expression.
    Expression* next = nary->first();
    for (size_t i = 0; i < nary->subsequent_length(); ++i) {
      AddArrowFunctionFormalParameters(parameters, next,
                                       nary->subsequent_op_position(i));
      next = nary->subsequent(i);
    }
    AddArrowFunctionFormalParameters(parameters, next, end_pos);
    return;
  }

  // For the binary case, we recurse on the left-hand side of binary comma
  // expressions.
  if (expr->IsBinaryOperation()) {
    BinaryOperation* binop = expr->AsBinaryOperation();
    // The classifier has already run, so we know that the expression is a valid
    // arrow function formals production.
    DCHECK_EQ(binop->op(), Token::COMMA);
    Expression* left = binop->left();
    Expression* right = binop->right();
    int comma_pos = binop->position();
    AddArrowFunctionFormalParameters(parameters, left, comma_pos);
    // LHS of comma expression should be unparenthesized.
    expr = right;
  }

  // Only the right-most expression may be a rest parameter.
  DCHECK(!parameters->has_rest);

  bool is_rest = expr->IsSpread();
  if (is_rest) {
    expr = expr->AsSpread()->expression();
    parameters->has_rest = true;
  }
  DCHECK_IMPLIES(parameters->is_simple, !is_rest);
  DCHECK_IMPLIES(parameters->is_simple, expr->IsVariableProxy());

  Expression* initializer = nullptr;
  if (expr->IsAssignment()) {
    Assignment* assignment = expr->AsAssignment();
    DCHECK(!assignment->IsCompoundAssignment());
    initializer = assignment->value();
    expr = assignment->target();
  }

  AddFormalParameter(parameters, expr, initializer, end_pos, is_rest);
}

void Parser::DeclareArrowFunctionFormalParameters(
    ParserFormalParameters* parameters, Expression* expr,
    const Scanner::Location& params_loc) {
  if (expr->IsEmptyParentheses() || has_error()) return;

  AddArrowFunctionFormalParameters(parameters, expr, params_loc.end_pos);

  if (parameters->arity > Code::kMaxArguments) {
    ReportMessageAt(params_loc, MessageTemplate::kMalformedArrowFunParamList);
    return;
  }

  DeclareFormalParameters(parameters);
  DCHECK_IMPLIES(parameters->is_simple,
                 parameters->scope->has_simple_parameters());
}

void Parser::PrepareGeneratorVariables() {
  // Calling a generator returns a generator object.  That object is stored
  // in a temporary variable, a definition that is used by "yield"
  // expressions.
  function_state_->scope()->DeclareGeneratorObjectVar(
      ast_value_factory()->dot_generator_object_string());
}

FunctionLiteral* Parser::ParseFunctionLiteral(
    const AstRawString* function_name, Scanner::Location function_name_location,
    FunctionNameValidity function_name_validity, FunctionKind kind,
    int function_token_pos, FunctionSyntaxKind function_syntax_kind,
    LanguageMode language_mode,
    ZonePtrList<const AstRawString>* arguments_for_wrapped_function) {
  // Function ::
  //   '(' FormalParameterList? ')' '{' FunctionBody '}'
  //
  // Getter ::
  //   '(' ')' '{' FunctionBody '}'
  //
  // Setter ::
  //   '(' PropertySetParameterList ')' '{' FunctionBody '}'

  bool is_wrapped = function_syntax_kind == FunctionSyntaxKind::kWrapped;
  DCHECK_EQ(is_wrapped, arguments_for_wrapped_function != nullptr);

  int pos = function_token_pos == kNoSourcePosition ? peek_position()
                                                    : function_token_pos;
  DCHECK_NE(kNoSourcePosition, pos);

  // Anonymous functions were passed either the empty symbol or a null
  // handle as the function name.  Remember if we were passed a non-empty
  // handle to decide whether to invoke function name inference.
  bool should_infer_name = function_name == nullptr;

  // We want a non-null handle as the function name by default. We will handle
  // the "function does not have a shared name" case later.
  if (should_infer_name) {
    function_name = ast_value_factory()->empty_string();
  }

  FunctionLiteral::EagerCompileHint eager_compile_hint =
      function_state_->next_function_is_likely_called() || is_wrapped
          ? FunctionLiteral::kShouldEagerCompile
          : default_eager_compile_hint();

  // Determine if the function can be parsed lazily. Lazy parsing is
  // different from lazy compilation; we need to parse more eagerly than we
  // compile.

  // We can only parse lazily if we also compile lazily. The heuristics for lazy
  // compilation are:
  // - It must not have been prohibited by the caller to Parse (some callers
  //   need a full AST).
  // - The outer scope must allow lazy compilation of inner functions.
  // - The function mustn't be a function expression with an open parenthesis
  //   before; we consider that a hint that the function will be called
  //   immediately, and it would be a waste of time to make it lazily
  //   compiled.
  // These are all things we can know at this point, without looking at the
  // function itself.

  // We separate between lazy parsing top level functions and lazy parsing inner
  // functions, because the latter needs to do more work. In particular, we need
  // to track unresolved variables to distinguish between these cases:
  // (function foo() {
  //   bar = function() { return 1; }
  //  })();
  // and
  // (function foo() {
  //   var a = 1;
  //   bar = function() { return a; }
  //  })();

  // Now foo will be parsed eagerly and compiled eagerly (optimization: assume
  // parenthesis before the function means that it will be called
  // immediately). bar can be parsed lazily, but we need to parse it in a mode
  // that tracks unresolved variables.
  DCHECK_IMPLIES(parse_lazily(), info()->allow_lazy_compile());
  DCHECK_IMPLIES(parse_lazily(), has_error() || allow_lazy_);
  DCHECK_IMPLIES(parse_lazily(), extension_ == nullptr);

  const bool is_lazy =
      eager_compile_hint == FunctionLiteral::kShouldLazyCompile;
  const bool is_top_level = AllowsLazyParsingWithoutUnresolvedVariables();
  const bool is_eager_top_level_function = !is_lazy && is_top_level;
  const bool is_lazy_top_level_function = is_lazy && is_top_level;
  const bool is_lazy_inner_function = is_lazy && !is_top_level;

  RuntimeCallTimerScope runtime_timer(
      runtime_call_stats_,
      parsing_on_main_thread_
          ? RuntimeCallCounterId::kParseFunctionLiteral
          : RuntimeCallCounterId::kParseBackgroundFunctionLiteral);
  base::ElapsedTimer timer;
  if (V8_UNLIKELY(FLAG_log_function_events)) timer.Start();

  // Determine whether we can still lazy parse the inner function.
  // The preconditions are:
  // - Lazy compilation has to be enabled.
  // - Neither V8 natives nor native function declarations can be allowed,
  //   since parsing one would retroactively force the function to be
  //   eagerly compiled.
  // - The invoker of this parser can't depend on the AST being eagerly
  //   built (either because the function is about to be compiled, or
  //   because the AST is going to be inspected for some reason).
  // - Because of the above, we can't be attempting to parse a
  //   FunctionExpression; even without enclosing parentheses it might be
  //   immediately invoked.
  // - The function literal shouldn't be hinted to eagerly compile.

  // Inner functions will be parsed using a temporary Zone. After parsing, we
  // will migrate unresolved variable into a Scope in the main Zone.

  const bool should_preparse_inner = parse_lazily() && is_lazy_inner_function;

  // If parallel compile tasks are enabled, and the function is an eager
  // top level function, then we can pre-parse the function and parse / compile
  // in a parallel task on a worker thread.
  bool should_post_parallel_task =
      parse_lazily() && is_eager_top_level_function &&
      FLAG_parallel_compile_tasks && info()->parallel_tasks() &&
      scanner()->stream()->can_be_cloned_for_parallel_access();

  // This may be modified later to reflect preparsing decision taken
  bool should_preparse = (parse_lazily() && is_lazy_top_level_function) ||
                         should_preparse_inner || should_post_parallel_task;

  ScopedPtrList<Statement> body(pointer_buffer());
  int expected_property_count = 0;
  int suspend_count = -1;
  int num_parameters = -1;
  int function_length = -1;
  bool has_duplicate_parameters = false;
  int function_literal_id = GetNextFunctionLiteralId();
  ProducedPreparseData* produced_preparse_data = nullptr;

  // This Scope lives in the main zone. We'll migrate data into that zone later.
  Zone* parse_zone = should_preparse ? &preparser_zone_ : zone();
  DeclarationScope* scope = NewFunctionScope(kind, parse_zone);
  SetLanguageMode(scope, language_mode);
#ifdef DEBUG
  scope->SetScopeName(function_name);
#endif

  if (!is_wrapped && V8_UNLIKELY(!Check(Token::LPAREN))) {
    ReportUnexpectedToken(Next());
    return nullptr;
  }
  scope->set_start_position(position());

  // Eager or lazy parse? If is_lazy_top_level_function, we'll parse
  // lazily. We'll call SkipFunction, which may decide to
  // abort lazy parsing if it suspects that wasn't a good idea. If so (in
  // which case the parser is expected to have backtracked), or if we didn't
  // try to lazy parse in the first place, we'll have to parse eagerly.
  bool did_preparse_successfully =
      should_preparse &&
      SkipFunction(function_name, kind, function_syntax_kind, scope,
                   &num_parameters, &function_length, &produced_preparse_data);

  if (!did_preparse_successfully) {
    // If skipping aborted, it rewound the scanner until before the LPAREN.
    // Consume it in that case.
    if (should_preparse) Consume(Token::LPAREN);
    should_post_parallel_task = false;
    ParseFunction(&body, function_name, pos, kind, function_syntax_kind, scope,
                  &num_parameters, &function_length, &has_duplicate_parameters,
                  &expected_property_count, &suspend_count,
                  arguments_for_wrapped_function);
  }

  if (V8_UNLIKELY(FLAG_log_function_events)) {
    double ms = timer.Elapsed().InMillisecondsF();
    const char* event_name =
        should_preparse
            ? (is_top_level ? "preparse-no-resolution" : "preparse-resolution")
            : "full-parse";
    logger_->FunctionEvent(
        event_name, script_id(), ms, scope->start_position(),
        scope->end_position(),
        reinterpret_cast<const char*>(function_name->raw_data()),
        function_name->byte_length());
  }
  if (V8_UNLIKELY(TracingFlags::is_runtime_stats_enabled()) &&
      did_preparse_successfully) {
    const RuntimeCallCounterId counters[2] = {
        RuntimeCallCounterId::kPreParseBackgroundWithVariableResolution,
        RuntimeCallCounterId::kPreParseWithVariableResolution};
    if (runtime_call_stats_) {
      runtime_call_stats_->CorrectCurrentCounterId(
          counters[parsing_on_main_thread_]);
    }
  }

  // Validate function name. We can do this only after parsing the function,
  // since the function can declare itself strict.
  language_mode = scope->language_mode();
  CheckFunctionName(language_mode, function_name, function_name_validity,
                    function_name_location);

  if (is_strict(language_mode)) {
    CheckStrictOctalLiteral(scope->start_position(), scope->end_position());
  }

  FunctionLiteral::ParameterFlag duplicate_parameters =
      has_duplicate_parameters ? FunctionLiteral::kHasDuplicateParameters
                               : FunctionLiteral::kNoDuplicateParameters;

  // Note that the FunctionLiteral needs to be created in the main Zone again.
  FunctionLiteral* function_literal = factory()->NewFunctionLiteral(
      function_name, scope, body, expected_property_count, num_parameters,
      function_length, duplicate_parameters, function_syntax_kind,
      eager_compile_hint, pos, true, function_literal_id,
      produced_preparse_data);
  function_literal->set_function_token_position(function_token_pos);
  function_literal->set_suspend_count(suspend_count);

  RecordFunctionLiteralSourceRange(function_literal);

  if (should_post_parallel_task) {
    // Start a parallel parse / compile task on the compiler dispatcher.
    info()->parallel_tasks()->Enqueue(info(), function_name, function_literal);
  }

  if (should_infer_name) {
    fni_.AddFunction(function_literal);
  }
  return function_literal;
}

bool Parser::SkipFunction(const AstRawString* function_name, FunctionKind kind,
                          FunctionSyntaxKind function_syntax_kind,
                          DeclarationScope* function_scope, int* num_parameters,
                          int* function_length,
                          ProducedPreparseData** produced_preparse_data) {
  FunctionState function_state(&function_state_, &scope_, function_scope);
  function_scope->set_zone(&preparser_zone_);

  DCHECK_NE(kNoSourcePosition, function_scope->start_position());
  DCHECK_EQ(kNoSourcePosition, parameters_end_pos_);

  DCHECK_IMPLIES(IsArrowFunction(kind),
                 scanner()->current_token() == Token::ARROW);

  // FIXME(marja): There are 2 ways to skip functions now. Unify them.
  if (consumed_preparse_data_) {
    int end_position;
    LanguageMode language_mode;
    int num_inner_functions;
    bool uses_super_property;
    if (stack_overflow()) return true;
    *produced_preparse_data =
        consumed_preparse_data_->GetDataForSkippableFunction(
            main_zone(), function_scope->start_position(), &end_position,
            num_parameters, function_length, &num_inner_functions,
            &uses_super_property, &language_mode);

    function_scope->outer_scope()->SetMustUsePreparseData();
    function_scope->set_is_skipped_function(true);
    function_scope->set_end_position(end_position);
    scanner()->SeekForward(end_position - 1);
    Expect(Token::RBRACE);
    SetLanguageMode(function_scope, language_mode);
    if (uses_super_property) {
      function_scope->RecordSuperPropertyUsage();
    }
    SkipFunctionLiterals(num_inner_functions);
    function_scope->ResetAfterPreparsing(ast_value_factory_, false);
    return true;
  }

  Scanner::BookmarkScope bookmark(scanner());
  bookmark.Set(function_scope->start_position());

  UnresolvedList::Iterator unresolved_private_tail;
  PrivateNameScopeIterator private_name_scope_iter(function_scope);
  if (!private_name_scope_iter.Done()) {
    unresolved_private_tail =
        private_name_scope_iter.GetScope()->GetUnresolvedPrivateNameTail();
  }

  // With no cached data, we partially parse the function, without building an
  // AST. This gathers the data needed to build a lazy function.
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.compile"), "V8.PreParse");

  PreParser::PreParseResult result = reusable_preparser()->PreParseFunction(
      function_name, kind, function_syntax_kind, function_scope, use_counts_,
      produced_preparse_data, this->script_id());

  if (result == PreParser::kPreParseStackOverflow) {
    // Propagate stack overflow.
    set_stack_overflow();
  } else if (pending_error_handler()->has_error_unidentifiable_by_preparser()) {
    // Make sure we don't re-preparse inner functions of the aborted function.
    // The error might be in an inner function.
    allow_lazy_ = false;
    mode_ = PARSE_EAGERLY;
    DCHECK(!pending_error_handler()->stack_overflow());
    // If we encounter an error that the preparser can not identify we reset to
    // the state before preparsing. The caller may then fully parse the function
    // to identify the actual error.
    bookmark.Apply();
    if (!private_name_scope_iter.Done()) {
      private_name_scope_iter.GetScope()->ResetUnresolvedPrivateNameTail(
          unresolved_private_tail);
    }
    function_scope->ResetAfterPreparsing(ast_value_factory_, true);
    pending_error_handler()->clear_unidentifiable_error();
    return false;
  } else if (pending_error_handler()->has_pending_error()) {
    DCHECK(!pending_error_handler()->stack_overflow());
    DCHECK(has_error());
  } else {
    DCHECK(!pending_error_handler()->stack_overflow());
    set_allow_eval_cache(reusable_preparser()->allow_eval_cache());

    PreParserLogger* logger = reusable_preparser()->logger();
    function_scope->set_end_position(logger->end());
    Expect(Token::RBRACE);
    total_preparse_skipped_ +=
        function_scope->end_position() - function_scope->start_position();
    *num_parameters = logger->num_parameters();
    *function_length = logger->function_length();
    SkipFunctionLiterals(logger->num_inner_functions());
    if (!private_name_scope_iter.Done()) {
      private_name_scope_iter.GetScope()->MigrateUnresolvedPrivateNameTail(
          factory(), unresolved_private_tail);
    }
    function_scope->AnalyzePartially(this, factory(), MaybeParsingArrowhead());
  }

  return true;
}

Block* Parser::BuildParameterInitializationBlock(
    const ParserFormalParameters& parameters) {
  DCHECK(!parameters.is_simple);
  DCHECK(scope()->is_function_scope());
  DCHECK_EQ(scope(), parameters.scope);
  ScopedPtrList<Statement> init_statements(pointer_buffer());
  int index = 0;
  for (auto parameter : parameters.params) {
    Expression* initial_value =
        factory()->NewVariableProxy(parameters.scope->parameter(index));
    if (parameter->initializer() != nullptr) {
      // IS_UNDEFINED($param) ? initializer : $param

      auto condition = factory()->NewCompareOperation(
          Token::EQ_STRICT,
          factory()->NewVariableProxy(parameters.scope->parameter(index)),
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);
      initial_value =
          factory()->NewConditional(condition, parameter->initializer(),
                                    initial_value, kNoSourcePosition);
    }

    DeclarationScope* param_scope = scope()->AsDeclarationScope();
    ScopedPtrList<Statement>* param_init_statements = &init_statements;

    base::Optional<ScopedPtrList<Statement>> non_simple_param_init_statements;
    if (!parameter->is_simple() && param_scope->sloppy_eval_can_extend_vars()) {
      param_scope = NewVarblockScope();
      param_scope->set_start_position(parameter->pattern->position());
      param_scope->set_end_position(parameter->initializer_end_position);
      param_scope->RecordEvalCall();
      non_simple_param_init_statements.emplace(pointer_buffer());
      param_init_statements = &non_simple_param_init_statements.value();
      // Rewrite the outer initializer to point to param_scope
      ReparentExpressionScope(stack_limit(), parameter->pattern, param_scope);
      ReparentExpressionScope(stack_limit(), initial_value, param_scope);
    }

    BlockState block_state(&scope_, param_scope);
    DeclarationParsingResult::Declaration decl(parameter->pattern,
                                               initial_value);

    InitializeVariables(param_init_statements, PARAMETER_VARIABLE, &decl);

    if (param_init_statements != &init_statements) {
      DCHECK_EQ(param_init_statements,
                &non_simple_param_init_statements.value());
      Block* param_block =
          factory()->NewBlock(true, *non_simple_param_init_statements);
      non_simple_param_init_statements.reset();
      param_block->set_scope(param_scope);
      param_scope = param_scope->FinalizeBlockScope()->AsDeclarationScope();
      init_statements.Add(param_block);
    }
    ++index;
  }
  return factory()->NewBlock(true, init_statements);
}

Scope* Parser::NewHiddenCatchScope() {
  Scope* catch_scope = NewScopeWithParent(scope(), CATCH_SCOPE);
  bool was_added;
  catch_scope->DeclareLocal(ast_value_factory()->dot_catch_string(),
                            VariableMode::kVar, NORMAL_VARIABLE, &was_added);
  DCHECK(was_added);
  catch_scope->set_is_hidden();
  return catch_scope;
}

Block* Parser::BuildRejectPromiseOnException(Block* inner_block) {
  // try {
  //   <inner_block>
  // } catch (.catch) {
  //   return %_AsyncFunctionReject(.generator_object, .catch, can_suspend);
  // }
  Block* result = factory()->NewBlock(1, true);

  // catch (.catch) {
  //   return %_AsyncFunctionReject(.generator_object, .catch, can_suspend)
  // }
  Scope* catch_scope = NewHiddenCatchScope();

  Expression* reject_promise;
  {
    ScopedPtrList<Expression> args(pointer_buffer());
    args.Add(factory()->NewVariableProxy(
        function_state_->scope()->generator_object_var()));
    args.Add(factory()->NewVariableProxy(catch_scope->catch_variable()));
    args.Add(factory()->NewBooleanLiteral(function_state_->CanSuspend(),
                                          kNoSourcePosition));
    reject_promise = factory()->NewCallRuntime(
        Runtime::kInlineAsyncFunctionReject, args, kNoSourcePosition);
  }
  Block* catch_block = IgnoreCompletion(
      factory()->NewReturnStatement(reject_promise, kNoSourcePosition));

  TryStatement* try_catch_statement =
      factory()->NewTryCatchStatementForAsyncAwait(
          inner_block, catch_scope, catch_block, kNoSourcePosition);
  result->statements()->Add(try_catch_statement, zone());
  return result;
}

Expression* Parser::BuildInitialYield(int pos, FunctionKind kind) {
  Expression* yield_result = factory()->NewVariableProxy(
      function_state_->scope()->generator_object_var());
  // The position of the yield is important for reporting the exception
  // caused by calling the .throw method on a generator suspended at the
  // initial yield (i.e. right after generator instantiation).
  function_state_->AddSuspend();
  return factory()->NewYield(yield_result, scope()->start_position(),
                             Suspend::kOnExceptionThrow);
}

void Parser::ParseFunction(
    ScopedPtrList<Statement>* body, const AstRawString* function_name, int pos,
    FunctionKind kind, FunctionSyntaxKind function_syntax_kind,
    DeclarationScope* function_scope, int* num_parameters, int* function_length,
    bool* has_duplicate_parameters, int* expected_property_count,
    int* suspend_count,
    ZonePtrList<const AstRawString>* arguments_for_wrapped_function) {
  ParsingModeScope mode(this, allow_lazy_ ? PARSE_LAZILY : PARSE_EAGERLY);

  FunctionState function_state(&function_state_, &scope_, function_scope);

  bool is_wrapped = function_syntax_kind == FunctionSyntaxKind::kWrapped;

  int expected_parameters_end_pos = parameters_end_pos_;
  if (expected_parameters_end_pos != kNoSourcePosition) {
    // This is the first function encountered in a CreateDynamicFunction eval.
    parameters_end_pos_ = kNoSourcePosition;
    // The function name should have been ignored, giving us the empty string
    // here.
    DCHECK_EQ(function_name, ast_value_factory()->empty_string());
  }

  ParserFormalParameters formals(function_scope);

  {
    ParameterDeclarationParsingScope formals_scope(this);
    if (is_wrapped) {
      // For a function implicitly wrapped in function header and footer, the
      // function arguments are provided separately to the source, and are
      // declared directly here.
      for (const AstRawString* arg : *arguments_for_wrapped_function) {
        const bool is_rest = false;
        Expression* argument = ExpressionFromIdentifier(arg, kNoSourcePosition);
        AddFormalParameter(&formals, argument, NullExpression(),
                           kNoSourcePosition, is_rest);
      }
      DCHECK_EQ(arguments_for_wrapped_function->length(),
                formals.num_parameters());
      DeclareFormalParameters(&formals);
    } else {
      // For a regular function, the function arguments are parsed from source.
      DCHECK_NULL(arguments_for_wrapped_function);
      ParseFormalParameterList(&formals);
      if (expected_parameters_end_pos != kNoSourcePosition) {
        // Check for '(' or ')' shenanigans in the parameter string for dynamic
        // functions.
        int position = peek_position();
        if (position < expected_parameters_end_pos) {
          ReportMessageAt(Scanner::Location(position, position + 1),
                          MessageTemplate::kArgStringTerminatesParametersEarly);
          return;
        } else if (position > expected_parameters_end_pos) {
          ReportMessageAt(Scanner::Location(expected_parameters_end_pos - 2,
                                            expected_parameters_end_pos),
                          MessageTemplate::kUnexpectedEndOfArgString);
          return;
        }
      }
      Expect(Token::RPAREN);
      int formals_end_position = scanner()->location().end_pos;

      CheckArityRestrictions(formals.arity, kind, formals.has_rest,
                             function_scope->start_position(),
                             formals_end_position);
      Expect(Token::LBRACE);
    }
    formals.duplicate_loc = formals_scope.duplicate_location();
  }

  *num_parameters = formals.num_parameters();
  *function_length = formals.function_length;

  AcceptINScope scope(this, true);
  ParseFunctionBody(body, function_name, pos, formals, kind,
                    function_syntax_kind, FunctionBodyType::kBlock);

  *has_duplicate_parameters = formals.has_duplicate();

  *expected_property_count = function_state.expected_property_count();
  *suspend_count = function_state.suspend_count();
}

void Parser::DeclareClassVariable(ClassScope* scope, const AstRawString* name,
                                  ClassInfo* class_info, int class_token_pos) {
#ifdef DEBUG
  scope->SetScopeName(name);
#endif

  DCHECK_IMPLIES(name == nullptr, class_info->is_anonymous);
  // Declare a special class variable for anonymous classes with the dot
  // if we need to save it for static private method access.
  Variable* class_variable =
      scope->DeclareClassVariable(ast_value_factory(), name, class_token_pos);
  Declaration* declaration = factory()->NewVariableDeclaration(class_token_pos);
  scope->declarations()->Add(declaration);
  declaration->set_var(class_variable);
}

// TODO(gsathya): Ideally, this should just bypass scope analysis and
// allocate a slot directly on the context. We should just store this
// index in the AST, instead of storing the variable.
Variable* Parser::CreateSyntheticContextVariable(const AstRawString* name) {
  VariableProxy* proxy =
      DeclareBoundVariable(name, VariableMode::kConst, kNoSourcePosition);
  proxy->var()->ForceContextAllocation();
  return proxy->var();
}

Variable* Parser::CreatePrivateNameVariable(ClassScope* scope,
                                            VariableMode mode,
                                            IsStaticFlag is_static_flag,
                                            const AstRawString* name) {
  DCHECK_NOT_NULL(name);
  int begin = position();
  int end = end_position();
  bool was_added = false;
  DCHECK(IsConstVariableMode(mode));
  Variable* var =
      scope->DeclarePrivateName(name, mode, is_static_flag, &was_added);
  if (!was_added) {
    Scanner::Location loc(begin, end);
    ReportMessageAt(loc, MessageTemplate::kVarRedeclaration, var->raw_name());
  }
  VariableProxy* proxy = factory()->NewVariableProxy(var, begin);
  return proxy->var();
}

void Parser::DeclarePublicClassField(ClassScope* scope,
                                     ClassLiteralProperty* property,
                                     bool is_static, bool is_computed_name,
                                     ClassInfo* class_info) {
  if (is_static) {
    class_info->static_fields->Add(property, zone());
  } else {
    class_info->instance_fields->Add(property, zone());
  }

  if (is_computed_name) {
    // We create a synthetic variable name here so that scope
    // analysis doesn't dedupe the vars.
    Variable* computed_name_var =
        CreateSyntheticContextVariable(ClassFieldVariableName(
            ast_value_factory(), class_info->computed_field_count));
    property->set_computed_name_var(computed_name_var);
    class_info->public_members->Add(property, zone());
  }
}

void Parser::DeclarePrivateClassMember(ClassScope* scope,
                                       const AstRawString* property_name,
                                       ClassLiteralProperty* property,
                                       ClassLiteralProperty::Kind kind,
                                       bool is_static, ClassInfo* class_info) {
  DCHECK_IMPLIES(kind != ClassLiteralProperty::Kind::FIELD,
                 allow_harmony_private_methods());

  if (kind == ClassLiteralProperty::Kind::FIELD) {
    if (is_static) {
      class_info->static_fields->Add(property, zone());
    } else {
      class_info->instance_fields->Add(property, zone());
    }
  }

  Variable* private_name_var = CreatePrivateNameVariable(
      scope, GetVariableMode(kind),
      is_static ? IsStaticFlag::kStatic : IsStaticFlag::kNotStatic,
      property_name);
  int pos = property->value()->position();
  if (pos == kNoSourcePosition) {
    pos = property->key()->position();
  }
  private_name_var->set_initializer_position(pos);
  property->set_private_name_var(private_name_var);
  class_info->private_members->Add(property, zone());
}

// This method declares a property of the given class.  It updates the
// following fields of class_info, as appropriate:
//   - constructor
//   - properties
void Parser::DeclarePublicClassMethod(const AstRawString* class_name,
                                      ClassLiteralProperty* property,
                                      bool is_constructor,
                                      ClassInfo* class_info) {
  if (is_constructor) {
    DCHECK(!class_info->constructor);
    class_info->constructor = property->value()->AsFunctionLiteral();
    DCHECK_NOT_NULL(class_info->constructor);
    class_info->constructor->set_raw_name(
        class_name != nullptr ? ast_value_factory()->NewConsString(class_name)
                              : nullptr);
    return;
  }

  class_info->public_members->Add(property, zone());
}

FunctionLiteral* Parser::CreateInitializerFunction(
    const char* name, DeclarationScope* scope,
    ZonePtrList<ClassLiteral::Property>* fields) {
  DCHECK_EQ(scope->function_kind(),
            FunctionKind::kClassMembersInitializerFunction);
  // function() { .. class fields initializer .. }
  ScopedPtrList<Statement> statements(pointer_buffer());
  InitializeClassMembersStatement* stmt =
      factory()->NewInitializeClassMembersStatement(fields, kNoSourcePosition);
  statements.Add(stmt);
  FunctionLiteral* result = factory()->NewFunctionLiteral(
      ast_value_factory()->GetOneByteString(name), scope, statements, 0, 0, 0,
      FunctionLiteral::kNoDuplicateParameters,
      FunctionSyntaxKind::kAccessorOrMethod,
      FunctionLiteral::kShouldEagerCompile, scope->start_position(), false,
      GetNextFunctionLiteralId());

  RecordFunctionLiteralSourceRange(result);

  return result;
}

// This method generates a ClassLiteral AST node.
// It uses the following fields of class_info:
//   - constructor (if missing, it updates it with a default constructor)
//   - proxy
//   - extends
//   - properties
//   - has_name_static_property
//   - has_static_computed_names
Expression* Parser::RewriteClassLiteral(ClassScope* block_scope,
                                        const AstRawString* name,
                                        ClassInfo* class_info, int pos,
                                        int end_pos) {
  DCHECK_NOT_NULL(block_scope);
  DCHECK_EQ(block_scope->scope_type(), CLASS_SCOPE);
  DCHECK_EQ(block_scope->language_mode(), LanguageMode::kStrict);

  bool has_extends = class_info->extends != nullptr;
  bool has_default_constructor = class_info->constructor == nullptr;
  if (has_default_constructor) {
    class_info->constructor =
        DefaultConstructor(name, has_extends, pos, end_pos);
  }

  if (name != nullptr) {
    DCHECK_NOT_NULL(block_scope->class_variable());
    block_scope->class_variable()->set_initializer_position(end_pos);
  }

  FunctionLiteral* static_fields_initializer = nullptr;
  if (class_info->has_static_class_fields) {
    static_fields_initializer = CreateInitializerFunction(
        "<static_fields_initializer>", class_info->static_fields_scope,
        class_info->static_fields);
  }

  FunctionLiteral* instance_members_initializer_function = nullptr;
  if (class_info->has_instance_members) {
    instance_members_initializer_function = CreateInitializerFunction(
        "<instance_members_initializer>", class_info->instance_members_scope,
        class_info->instance_fields);
    class_info->constructor->set_requires_instance_members_initializer(true);
    class_info->constructor->add_expected_properties(
        class_info->instance_fields->length());
  }

  ClassLiteral* class_literal = factory()->NewClassLiteral(
      block_scope, class_info->extends, class_info->constructor,
      class_info->public_members, class_info->private_members,
      static_fields_initializer, instance_members_initializer_function, pos,
      end_pos, class_info->has_name_static_property,
      class_info->has_static_computed_names, class_info->is_anonymous,
      class_info->has_private_methods);

  AddFunctionForNameInference(class_info->constructor);
  return class_literal;
}

void Parser::InsertShadowingVarBindingInitializers(Block* inner_block) {
  // For each var-binding that shadows a parameter, insert an assignment
  // initializing the variable with the parameter.
  Scope* inner_scope = inner_block->scope();
  DCHECK(inner_scope->is_declaration_scope());
  Scope* function_scope = inner_scope->outer_scope();
  DCHECK(function_scope->is_function_scope());
  BlockState block_state(&scope_, inner_scope);
  for (Declaration* decl : *inner_scope->declarations()) {
    if (decl->var()->mode() != VariableMode::kVar ||
        !decl->IsVariableDeclaration()) {
      continue;
    }
    const AstRawString* name = decl->var()->raw_name();
    Variable* parameter = function_scope->LookupLocal(name);
    if (parameter == nullptr) continue;
    VariableProxy* to = NewUnresolved(name);
    VariableProxy* from = factory()->NewVariableProxy(parameter);
    Expression* assignment =
        factory()->NewAssignment(Token::ASSIGN, to, from, kNoSourcePosition);
    Statement* statement =
        factory()->NewExpressionStatement(assignment, kNoSourcePosition);
    inner_block->statements()->InsertAt(0, statement, zone());
  }
}

void Parser::InsertSloppyBlockFunctionVarBindings(DeclarationScope* scope) {
  // For the outermost eval scope, we cannot hoist during parsing: let
  // declarations in the surrounding scope may prevent hoisting, but the
  // information is unaccessible during parsing. In this case, we hoist later in
  // DeclarationScope::Analyze.
  if (scope->is_eval_scope() && scope->outer_scope() == original_scope_) {
    return;
  }
  scope->HoistSloppyBlockFunctions(factory());
}

// ----------------------------------------------------------------------------
// Parser support

bool Parser::TargetStackContainsLabel(const AstRawString* label) {
  for (ParserTarget* t = target_stack_; t != nullptr; t = t->previous()) {
    if (ContainsLabel(t->statement()->labels(), label)) return true;
  }
  return false;
}

BreakableStatement* Parser::LookupBreakTarget(const AstRawString* label) {
  bool anonymous = label == nullptr;
  for (ParserTarget* t = target_stack_; t != nullptr; t = t->previous()) {
    BreakableStatement* stat = t->statement();
    if ((anonymous && stat->is_target_for_anonymous()) ||
        (!anonymous && ContainsLabel(stat->labels(), label))) {
      return stat;
    }
  }
  return nullptr;
}

IterationStatement* Parser::LookupContinueTarget(const AstRawString* label) {
  bool anonymous = label == nullptr;
  for (ParserTarget* t = target_stack_; t != nullptr; t = t->previous()) {
    IterationStatement* stat = t->statement()->AsIterationStatement();
    if (stat == nullptr) continue;

    DCHECK(stat->is_target_for_anonymous());
    if (anonymous || ContainsLabel(stat->own_labels(), label)) {
      return stat;
    }
    if (ContainsLabel(stat->labels(), label)) break;
  }
  return nullptr;
}

void Parser::HandleSourceURLComments(Isolate* isolate, Handle<Script> script) {
  Handle<String> source_url = scanner_.SourceUrl(isolate);
  if (!source_url.is_null()) {
    script->set_source_url(*source_url);
  }
  Handle<String> source_mapping_url = scanner_.SourceMappingUrl(isolate);
  if (!source_mapping_url.is_null()) {
    script->set_source_mapping_url(*source_mapping_url);
  }
}

void Parser::UpdateStatistics(Isolate* isolate, Handle<Script> script) {
  // Move statistics to Isolate.
  for (int feature = 0; feature < v8::Isolate::kUseCounterFeatureCount;
       ++feature) {
    if (use_counts_[feature] > 0) {
      isolate->CountUsage(v8::Isolate::UseCounterFeature(feature));
    }
  }
  if (scanner_.FoundHtmlComment()) {
    isolate->CountUsage(v8::Isolate::kHtmlComment);
    if (script->line_offset() == 0 && script->column_offset() == 0) {
      isolate->CountUsage(v8::Isolate::kHtmlCommentInExternalScript);
    }
  }
  isolate->counters()->total_preparse_skipped()->Increment(
      total_preparse_skipped_);
}

void Parser::ParseOnBackground(ParseInfo* info) {
  RuntimeCallTimerScope runtimeTimer(
      runtime_call_stats_, RuntimeCallCounterId::kParseBackgroundProgram);
  parsing_on_main_thread_ = false;
  set_script_id(info->script_id());

  DCHECK_NULL(info->literal());
  FunctionLiteral* result = nullptr;

  scanner_.Initialize();
  DCHECK(info->maybe_outer_scope_info().is_null());

  DCHECK(original_scope_);

  // When streaming, we don't know the length of the source until we have parsed
  // it. The raw data can be UTF-8, so we wouldn't know the source length until
  // we have decoded it anyway even if we knew the raw data length (which we
  // don't). We work around this by storing all the scopes which need their end
  // position set at the end of the script (the top scope and possible eval
  // scopes) and set their end position after we know the script length.
  if (info->is_toplevel()) {
    result = DoParseProgram(/* isolate = */ nullptr, info);
  } else {
    result =
        DoParseFunction(/* isolate = */ nullptr, info, info->function_name());
  }
  MaybeResetCharacterStream(info, result);

  info->set_literal(result);

  // We cannot internalize on a background thread; a foreground task will take
  // care of calling AstValueFactory::Internalize just before compilation.
}

Parser::TemplateLiteralState Parser::OpenTemplateLiteral(int pos) {
  return new (zone()) TemplateLiteral(zone(), pos);
}

void Parser::AddTemplateSpan(TemplateLiteralState* state, bool should_cook,
                             bool tail) {
  int end = scanner()->location().end_pos - (tail ? 1 : 2);
  const AstRawString* raw = scanner()->CurrentRawSymbol(ast_value_factory());
  if (should_cook) {
    const AstRawString* cooked = scanner()->CurrentSymbol(ast_value_factory());
    (*state)->AddTemplateSpan(cooked, raw, end, zone());
  } else {
    (*state)->AddTemplateSpan(nullptr, raw, end, zone());
  }
}

void Parser::AddTemplateExpression(TemplateLiteralState* state,
                                   Expression* expression) {
  (*state)->AddExpression(expression, zone());
}

Expression* Parser::CloseTemplateLiteral(TemplateLiteralState* state, int start,
                                         Expression* tag) {
  TemplateLiteral* lit = *state;
  int pos = lit->position();
  const ZonePtrList<const AstRawString>* cooked_strings = lit->cooked();
  const ZonePtrList<const AstRawString>* raw_strings = lit->raw();
  const ZonePtrList<Expression>* expressions = lit->expressions();
  DCHECK_EQ(cooked_strings->length(), raw_strings->length());
  DCHECK_EQ(cooked_strings->length(), expressions->length() + 1);

  if (!tag) {
    if (cooked_strings->length() == 1) {
      return factory()->NewStringLiteral(cooked_strings->first(), pos);
    }
    return factory()->NewTemplateLiteral(cooked_strings, expressions, pos);
  } else {
    // GetTemplateObject
    Expression* template_object =
        factory()->NewGetTemplateObject(cooked_strings, raw_strings, pos);

    // Call TagFn
    ScopedPtrList<Expression> call_args(pointer_buffer());
    call_args.Add(template_object);
    call_args.AddAll(*expressions);
    return factory()->NewTaggedTemplate(tag, call_args, pos);
  }
}

namespace {

bool OnlyLastArgIsSpread(const ScopedPtrList<Expression>& args) {
  for (int i = 0; i < args.length() - 1; i++) {
    if (args.at(i)->IsSpread()) {
      return false;
    }
  }
  return args.at(args.length() - 1)->IsSpread();
}

}  // namespace

ArrayLiteral* Parser::ArrayLiteralFromListWithSpread(
    const ScopedPtrList<Expression>& list) {
  // If there's only a single spread argument, a fast path using CallWithSpread
  // is taken.
  DCHECK_LT(1, list.length());

  // The arguments of the spread call become a single ArrayLiteral.
  int first_spread = 0;
  for (; first_spread < list.length() && !list.at(first_spread)->IsSpread();
       ++first_spread) {
  }

  DCHECK_LT(first_spread, list.length());
  return factory()->NewArrayLiteral(list, first_spread, kNoSourcePosition);
}

Expression* Parser::SpreadCall(Expression* function,
                               const ScopedPtrList<Expression>& args_list,
                               int pos, Call::PossiblyEval is_possibly_eval,
                               bool optional_chain) {
  // Handle this case in BytecodeGenerator.
  if (OnlyLastArgIsSpread(args_list) || function->IsSuperCallReference()) {
    return factory()->NewCall(function, args_list, pos, Call::NOT_EVAL,
                              optional_chain);
  }

  ScopedPtrList<Expression> args(pointer_buffer());
  if (function->IsProperty()) {
    // Method calls
    if (function->AsProperty()->IsSuperAccess()) {
      Expression* home = ThisExpression();
      args.Add(function);
      args.Add(home);
    } else {
      Variable* temp = NewTemporary(ast_value_factory()->empty_string());
      VariableProxy* obj = factory()->NewVariableProxy(temp);
      Assignment* assign_obj = factory()->NewAssignment(
          Token::ASSIGN, obj, function->AsProperty()->obj(), kNoSourcePosition);
      function =
          factory()->NewProperty(assign_obj, function->AsProperty()->key(),
                                 kNoSourcePosition, optional_chain);
      args.Add(function);
      obj = factory()->NewVariableProxy(temp);
      args.Add(obj);
    }
  } else {
    // Non-method calls
    args.Add(function);
    args.Add(factory()->NewUndefinedLiteral(kNoSourcePosition));
  }
  args.Add(ArrayLiteralFromListWithSpread(args_list));
  return factory()->NewCallRuntime(Context::REFLECT_APPLY_INDEX, args, pos);
}

Expression* Parser::SpreadCallNew(Expression* function,
                                  const ScopedPtrList<Expression>& args_list,
                                  int pos) {
  if (OnlyLastArgIsSpread(args_list)) {
    // Handle in BytecodeGenerator.
    return factory()->NewCallNew(function, args_list, pos);
  }
  ScopedPtrList<Expression> args(pointer_buffer());
  args.Add(function);
  args.Add(ArrayLiteralFromListWithSpread(args_list));

  return factory()->NewCallRuntime(Context::REFLECT_CONSTRUCT_INDEX, args, pos);
}

void Parser::SetLanguageMode(Scope* scope, LanguageMode mode) {
  v8::Isolate::UseCounterFeature feature;
  if (is_sloppy(mode))
    feature = v8::Isolate::kSloppyMode;
  else if (is_strict(mode))
    feature = v8::Isolate::kStrictMode;
  else
    UNREACHABLE();
  ++use_counts_[feature];
  scope->SetLanguageMode(mode);
}

void Parser::SetAsmModule() {
  // Store the usage count; The actual use counter on the isolate is
  // incremented after parsing is done.
  ++use_counts_[v8::Isolate::kUseAsm];
  DCHECK(scope()->is_declaration_scope());
  scope()->AsDeclarationScope()->set_is_asm_module();
  info_->set_contains_asm_module(true);
}

Expression* Parser::ExpressionListToExpression(
    const ScopedPtrList<Expression>& args) {
  Expression* expr = args.at(0);
  if (args.length() == 1) return expr;
  if (args.length() == 2) {
    return factory()->NewBinaryOperation(Token::COMMA, expr, args.at(1),
                                         args.at(1)->position());
  }
  NaryOperation* result =
      factory()->NewNaryOperation(Token::COMMA, expr, args.length() - 1);
  for (int i = 1; i < args.length(); i++) {
    result->AddSubsequent(args.at(i), args.at(i)->position());
  }
  return result;
}

// This method completes the desugaring of the body of async_function.
void Parser::RewriteAsyncFunctionBody(ScopedPtrList<Statement>* body,
                                      Block* block, Expression* return_value) {
  // function async_function() {
  //   .generator_object = %_AsyncFunctionEnter();
  //   BuildRejectPromiseOnException({
  //     ... block ...
  //     return %_AsyncFunctionResolve(.generator_object, expr);
  //   })
  // }

  block->statements()->Add(factory()->NewSyntheticAsyncReturnStatement(
                               return_value, return_value->position()),
                           zone());
  block = BuildRejectPromiseOnException(block);
  body->Add(block);
}

void Parser::SetFunctionNameFromPropertyName(LiteralProperty* property,
                                             const AstRawString* name,
                                             const AstRawString* prefix) {
  if (has_error()) return;
  // Ensure that the function we are going to create has shared name iff
  // we are not going to set it later.
  if (property->NeedsSetFunctionName()) {
    name = nullptr;
    prefix = nullptr;
  } else {
    // If the property value is an anonymous function or an anonymous class or
    // a concise method or an accessor function which doesn't require the name
    // to be set then the shared name must be provided.
    DCHECK_IMPLIES(property->value()->IsAnonymousFunctionDefinition() ||
                       property->value()->IsConciseMethodDefinition() ||
                       property->value()->IsAccessorFunctionDefinition(),
                   name != nullptr);
  }

  Expression* value = property->value();
  SetFunctionName(value, name, prefix);
}

void Parser::SetFunctionNameFromPropertyName(ObjectLiteralProperty* property,
                                             const AstRawString* name,
                                             const AstRawString* prefix) {
  // Ignore "__proto__" as a name when it's being used to set the [[Prototype]]
  // of an object literal.
  // See ES #sec-__proto__-property-names-in-object-initializers.
  if (property->IsPrototype() || has_error()) return;

  DCHECK(!property->value()->IsAnonymousFunctionDefinition() ||
         property->kind() == ObjectLiteralProperty::COMPUTED);

  SetFunctionNameFromPropertyName(static_cast<LiteralProperty*>(property), name,
                                  prefix);
}

void Parser::SetFunctionNameFromIdentifierRef(Expression* value,
                                              Expression* identifier) {
  if (!identifier->IsVariableProxy()) return;
  SetFunctionName(value, identifier->AsVariableProxy()->raw_name());
}

void Parser::SetFunctionName(Expression* value, const AstRawString* name,
                             const AstRawString* prefix) {
  if (!value->IsAnonymousFunctionDefinition() &&
      !value->IsConciseMethodDefinition() &&
      !value->IsAccessorFunctionDefinition()) {
    return;
  }
  auto function = value->AsFunctionLiteral();
  if (value->IsClassLiteral()) {
    function = value->AsClassLiteral()->constructor();
  }
  if (function != nullptr) {
    AstConsString* cons_name = nullptr;
    if (name != nullptr) {
      if (prefix != nullptr) {
        cons_name = ast_value_factory()->NewConsString(prefix, name);
      } else {
        cons_name = ast_value_factory()->NewConsString(name);
      }
    } else {
      DCHECK_NULL(prefix);
    }
    function->set_raw_name(cons_name);
  }
}

Statement* Parser::CheckCallable(Variable* var, Expression* error, int pos) {
  const int nopos = kNoSourcePosition;
  Statement* validate_var;
  {
    Expression* type_of = factory()->NewUnaryOperation(
        Token::TYPEOF, factory()->NewVariableProxy(var), nopos);
    Expression* function_literal = factory()->NewStringLiteral(
        ast_value_factory()->function_string(), nopos);
    Expression* condition = factory()->NewCompareOperation(
        Token::EQ_STRICT, type_of, function_literal, nopos);

    Statement* throw_call = factory()->NewExpressionStatement(error, pos);

    validate_var = factory()->NewIfStatement(
        condition, factory()->EmptyStatement(), throw_call, nopos);
  }
  return validate_var;
}

}  // namespace internal
}  // namespace v8
