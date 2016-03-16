// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Changes Blink-style names to Chrome-style names. Currently transforms:
//   fields:
//     int m_operationCount => int operation_count_
//   variables (including parameters):
//     int mySuperVariable => int my_super_variable
//   constants:
//     const int maxThings => const int kMaxThings
//   free functions and methods:
//     void doThisThenThat() => void DoThisAndThat()

#include <assert.h>
#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/ASTMatchers/ASTMatchersMacros.h"
#include "clang/Basic/CharInfo.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetSelect.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/file.h>
#include <unistd.h>
#endif

using namespace clang::ast_matchers;
using clang::tooling::CommonOptionsParser;
using clang::tooling::Replacement;
using clang::tooling::Replacements;
using llvm::StringRef;

namespace {

// Hack: prevent the custom isDefaulted() from conflicting with the one defined
// in newer revisions of Clang.
namespace internal_hack {

// This is available in newer clang revisions... but alas, Chrome has not rolled
// that far yet.
AST_MATCHER(clang::FunctionDecl, isDefaulted) {
  return Node.isDefaulted();
}

}  // namespace internal_hack

const char kBlinkFieldPrefix[] = "m_";
const char kBlinkStaticMemberPrefix[] = "s_";
const char kGeneratedFileRegex[] = "^gen/|/gen/";

AST_MATCHER(clang::FunctionDecl, isOverloadedOperator) {
  return Node.isOverloadedOperator();
}

AST_MATCHER_P(clang::FunctionTemplateDecl,
              templatedDecl,
              clang::ast_matchers::internal::Matcher<clang::FunctionDecl>,
              InnerMatcher) {
  return InnerMatcher.matches(*Node.getTemplatedDecl(), Finder, Builder);
}

// This will narrow CXXCtorInitializers down for both FieldDecls and
// IndirectFieldDecls (ie. anonymous unions and such). In both cases
// getAnyMember() will return a FieldDecl which we can match against.
AST_MATCHER_P(clang::CXXCtorInitializer,
              forAnyField,
              clang::ast_matchers::internal::Matcher<clang::FieldDecl>,
              InnerMatcher) {
  const clang::FieldDecl* NodeAsDecl = Node.getAnyMember();
  return (NodeAsDecl != nullptr &&
          InnerMatcher.matches(*NodeAsDecl, Finder, Builder));
}

bool IsDeclContextInWTF(const clang::DeclContext* decl_context) {
  auto* namespace_decl = clang::dyn_cast_or_null<clang::NamespaceDecl>(
      decl_context->getEnclosingNamespaceContext());
  if (!namespace_decl)
    return false;
  if (namespace_decl->getParent()->isTranslationUnit() &&
      namespace_decl->getName() == "WTF")
    return true;
  return IsDeclContextInWTF(namespace_decl->getParent());
}

template <typename T>
bool MatchAllOverriddenMethods(
    const clang::CXXMethodDecl& decl,
    T&& inner_matcher,
    clang::ast_matchers::internal::ASTMatchFinder* finder,
    clang::ast_matchers::internal::BoundNodesTreeBuilder* builder) {
  bool override_matches = false;
  bool override_not_matches = false;

  for (auto it = decl.begin_overridden_methods();
       it != decl.end_overridden_methods(); ++it) {
    if (MatchAllOverriddenMethods(**it, inner_matcher, finder, builder))
      override_matches = true;
    else
      override_not_matches = true;
  }

  // If this fires we have a class overriding a method that matches, and a
  // method that does not match the inner matcher. In that case we will match
  // one ancestor method but not the other. If we rename one of the and not the
  // other it will break what this class overrides, disconnecting it from the
  // one we did not rename which creates a behaviour change. So assert and
  // demand the user to fix the code first (or add the method to our
  // blacklist T_T).
  if (override_matches || override_not_matches)
    assert(override_matches != override_not_matches);

  // If the method overrides something that doesn't match, so the method itself
  // doesn't match.
  if (override_not_matches)
    return false;
  // If the method overrides something that matches, so the method ifself
  // matches.
  if (override_matches)
    return true;

  return inner_matcher.matches(decl, finder, builder);
}

AST_MATCHER_P(clang::CXXMethodDecl,
              includeAllOverriddenMethods,
              clang::ast_matchers::internal::Matcher<clang::CXXMethodDecl>,
              InnerMatcher) {
  return MatchAllOverriddenMethods(Node, InnerMatcher, Finder, Builder);
}

bool IsMethodOverrideOf(const clang::CXXMethodDecl& decl,
                        const char* class_name) {
  if (decl.getParent()->getQualifiedNameAsString() == class_name)
    return true;
  for (auto it = decl.begin_overridden_methods();
       it != decl.end_overridden_methods(); ++it) {
    if (IsMethodOverrideOf(**it, class_name))
      return true;
  }
  return false;
}

bool IsBlacklistedMethod(const clang::CXXMethodDecl& decl) {
  if (decl.isStatic())
    return false;

  clang::StringRef name = decl.getName();

  // These methods should never be renamed.
  static const char* kBlacklistMethods[] = {"trace", "lock", "unlock",
                                            "try_lock"};
  for (const auto& b : kBlacklistMethods) {
    if (name == b)
      return true;
  }

  // Iterator methods shouldn't be renamed to work with stl and range-for
  // loops.
  std::string ret_type = decl.getReturnType().getAsString();
  if (ret_type.find("iterator") != std::string::npos ||
      ret_type.find("Iterator") != std::string::npos) {
    static const char* kIteratorBlacklist[] = {"begin", "end", "rbegin",
                                               "rend"};
    for (const auto& b : kIteratorBlacklist) {
      if (name == b)
        return true;
    }
  }

  // Subclasses of InspectorAgent will subclass "disable()" from both blink and
  // from gen/, which is problematic, but DevTools folks don't want to rename
  // it or split this up. So don't rename it at all.
  if (name.equals("disable") &&
      IsMethodOverrideOf(decl, "blink::InspectorAgent"))
    return true;

  return false;
}

AST_MATCHER(clang::CXXMethodDecl, isBlacklistedMethod) {
  return IsBlacklistedMethod(Node);
}

// Helper to convert from a camelCaseName to camel_case_name. It uses some
// heuristics to try to handle acronyms in camel case names correctly.
std::string CamelCaseToUnderscoreCase(StringRef input) {
  std::string output;
  bool needs_underscore = false;
  bool was_lowercase = false;
  bool was_number = false;
  bool was_uppercase = false;
  bool first_char = true;
  // Iterate in reverse to minimize the amount of backtracking.
  for (const unsigned char* i = input.bytes_end() - 1; i >= input.bytes_begin();
       --i) {
    char c = *i;
    bool is_lowercase = clang::isLowercase(c);
    bool is_number = clang::isDigit(c);
    bool is_uppercase = clang::isUppercase(c);
    c = clang::toLowercase(c);
    // Transitioning from upper to lower case requires an underscore. This is
    // needed to handle names with acronyms, e.g. handledHTTPRequest needs a '_'
    // in 'dH'. This is a complement to the non-acronym case further down.
    if (was_uppercase && is_lowercase)
      needs_underscore = true;
    if (needs_underscore) {
      output += '_';
      needs_underscore = false;
    }
    output += c;
    // Handles the non-acronym case: transitioning from lower to upper case
    // requires an underscore when emitting the next character, e.g. didLoad
    // needs a '_' in 'dL'.
    if (!first_char && was_lowercase && is_uppercase)
      needs_underscore = true;
    was_lowercase = is_lowercase;
    was_number = is_number;
    was_uppercase = is_uppercase;
    first_char = false;
  }
  std::reverse(output.begin(), output.end());
  return output;
}

bool IsProbablyConst(const clang::VarDecl& decl,
                     const clang::ASTContext& context) {
  clang::QualType type = decl.getType();
  if (!type.isConstQualified())
    return false;

  if (type.isVolatileQualified())
    return false;

  // http://google.github.io/styleguide/cppguide.html#Constant_Names
  // Static variables that are const-qualified should use kConstantStyle naming.
  if (decl.getStorageDuration() == clang::SD_Static)
    return true;

  const clang::Expr* initializer = decl.getInit();
  if (!initializer)
    return false;

  // If the expression is dependent on a template input, then we are not
  // sure if it can be compile-time generated as calling isEvaluatable() is
  // not valid on |initializer|.
  // TODO(crbug.com/581218): We could probably look at each compiled
  // instantiation of the template and see if they are all compile-time
  // isEvaluable().
  if (initializer->isInstantiationDependent())
    return false;

  // If the expression can be evaluated at compile time, then it should have a
  // kFoo style name. Otherwise, not.
  return initializer->isEvaluatable(context);
}

bool GetNameForDecl(const clang::FunctionDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  StringRef original_name = decl.getName();

  // Some functions shouldn't be renamed because reasons.
  // - swap() methods should match the signature of std::swap for ADL tricks.
  static const char* kBlacklist[] = {"swap"};
  for (const auto& b : kBlacklist) {
    if (original_name == b)
      return false;
  }

  name = original_name.str();
  name[0] = clang::toUppercase(name[0]);
  return true;
}

bool GetNameForDecl(const clang::EnumConstantDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  StringRef original_name = decl.getName();

  // If it's already correct leave it alone.
  if (original_name.size() >= 2 && original_name[0] == 'k' &&
      clang::isUppercase(original_name[1]))
    return false;

  bool is_shouty = true;
  for (char c : original_name) {
    if (!clang::isUppercase(c) && !clang::isDigit(c) && c != '_') {
      is_shouty = false;
      break;
    }
  }

  if (is_shouty)
    return false;

  name = 'k';  // k prefix on enum values.
  name += original_name;
  name[1] = clang::toUppercase(name[1]);
  return true;
}

bool GetNameForDecl(const clang::CXXMethodDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  name = decl.getName().str();
  name[0] = clang::toUppercase(name[0]);
  return true;
}

bool GetNameForDecl(const clang::FieldDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  StringRef original_name = decl.getName();
  bool member_prefix = original_name.startswith(kBlinkFieldPrefix);

  StringRef rename_part = !member_prefix
                              ? original_name
                              : original_name.substr(strlen(kBlinkFieldPrefix));
  name = CamelCaseToUnderscoreCase(rename_part);

  // Assume that prefix of m_ was intentional and always replace it with a
  // suffix _.
  if (member_prefix && name.back() != '_')
    name += '_';

  return true;
}

bool GetNameForDecl(const clang::VarDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  StringRef original_name = decl.getName();

  // Nothing to do for unnamed parameters.
  if (clang::isa<clang::ParmVarDecl>(decl) && original_name.empty())
    return false;

  // static class members match against VarDecls. Blink style dictates that
  // these should be prefixed with `s_`, so strip that off. Also check for `m_`
  // and strip that off too, for code that accidentally uses the wrong prefix.
  if (original_name.startswith(kBlinkStaticMemberPrefix))
    original_name = original_name.substr(strlen(kBlinkStaticMemberPrefix));
  else if (original_name.startswith(kBlinkFieldPrefix))
    original_name = original_name.substr(strlen(kBlinkFieldPrefix));

  bool is_const = IsProbablyConst(decl, context);
  if (is_const) {
    // Don't try to rename constants that already conform to Chrome style.
    if (original_name.size() >= 2 && original_name[0] == 'k' &&
        clang::isUppercase(original_name[1]))
      return false;

    // Struct consts in WTF do not become kFoo cuz stuff like type traits
    // should stay as lowercase.
    const clang::DeclContext* decl_context = decl.getDeclContext();
    bool is_in_wtf = IsDeclContextInWTF(decl_context);
    const clang::CXXRecordDecl* parent =
        clang::dyn_cast_or_null<clang::CXXRecordDecl>(decl_context);
    if (is_in_wtf && parent && parent->isStruct())
      return false;

    name = 'k';
    name.append(original_name.data(), original_name.size());
    name[1] = clang::toUppercase(name[1]);
  } else {
    name = CamelCaseToUnderscoreCase(original_name);
  }

  // Static members end with _ just like other members, but constants should
  // not.
  if (!is_const && decl.isStaticDataMember()) {
    name += '_';
  }

  return true;
}

bool GetNameForDecl(const clang::FunctionTemplateDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  clang::FunctionDecl* templated_function = decl.getTemplatedDecl();
  if (auto* method = clang::dyn_cast<clang::CXXMethodDecl>(templated_function))
    return GetNameForDecl(*method, context, name);
  return GetNameForDecl(*templated_function, context, name);
}

bool GetNameForDecl(const clang::UsingDecl& decl,
                    const clang::ASTContext& context,
                    std::string& name) {
  assert(decl.shadow_size() > 0);

  // If a using declaration's targeted declaration is a set of overloaded
  // functions, it can introduce multiple shadowed declarations. Just using the
  // first one is OK, since overloaded functions have the same name, by
  // definition.
  clang::NamedDecl* shadowed_name = decl.shadow_begin()->getTargetDecl();
  // Note: CXXMethodDecl must be checked before FunctionDecl, because
  // CXXMethodDecl is derived from FunctionDecl.
  if (auto* method = clang::dyn_cast<clang::CXXMethodDecl>(shadowed_name))
    return GetNameForDecl(*method, context, name);
  if (auto* function = clang::dyn_cast<clang::FunctionDecl>(shadowed_name))
    return GetNameForDecl(*function, context, name);
  if (auto* var = clang::dyn_cast<clang::VarDecl>(shadowed_name))
    return GetNameForDecl(*var, context, name);
  if (auto* field = clang::dyn_cast<clang::FieldDecl>(shadowed_name))
    return GetNameForDecl(*field, context, name);
  if (auto* function_template =
          clang::dyn_cast<clang::FunctionTemplateDecl>(shadowed_name))
    return GetNameForDecl(*function_template, context, name);
  if (auto* enumc = clang::dyn_cast<clang::EnumConstantDecl>(shadowed_name))
    return GetNameForDecl(*enumc, context, name);

  return false;
}

template <typename Type>
struct TargetNodeTraits;

template <>
struct TargetNodeTraits<clang::NamedDecl> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::NamedDecl& decl) {
    return decl.getLocation();
  }
  static const char* GetType() { return "NamedDecl"; }
};
const char TargetNodeTraits<clang::NamedDecl>::kName[] = "decl";

template <>
struct TargetNodeTraits<clang::MemberExpr> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::MemberExpr& expr) {
    return expr.getMemberLoc();
  }
  static const char* GetType() { return "MemberExpr"; }
};
const char TargetNodeTraits<clang::MemberExpr>::kName[] = "expr";

template <>
struct TargetNodeTraits<clang::DeclRefExpr> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::DeclRefExpr& expr) {
    return expr.getLocation();
  }
  static const char* GetType() { return "DeclRefExpr"; }
};
const char TargetNodeTraits<clang::DeclRefExpr>::kName[] = "expr";

template <>
struct TargetNodeTraits<clang::CXXCtorInitializer> {
  static const char kName[];
  static clang::SourceLocation GetLoc(const clang::CXXCtorInitializer& init) {
    assert(init.isWritten());
    return init.getSourceLocation();
  }
  static const char* GetType() { return "CXXCtorInitializer"; }
};
const char TargetNodeTraits<clang::CXXCtorInitializer>::kName[] = "initializer";

template <typename DeclNode, typename TargetNode>
class RewriterBase : public MatchFinder::MatchCallback {
 public:
  explicit RewriterBase(Replacements* replacements)
      : replacements_(replacements) {}

  void run(const MatchFinder::MatchResult& result) override {
    const DeclNode* decl = result.Nodes.getNodeAs<DeclNode>("decl");
    // If false, there's no name to be renamed.
    if (!decl->getIdentifier())
      return;
    clang::SourceLocation decl_loc =
        TargetNodeTraits<clang::NamedDecl>::GetLoc(*decl);
    if (decl_loc.isMacroID()) {
      // Get the location of the spelling of the declaration. If token pasting
      // was used this will be in "scratch space" and we don't know how to get
      // from there back to/ the actual macro with the foo##bar text. So just
      // don't replace in that case.
      clang::SourceLocation spell =
          result.SourceManager->getSpellingLoc(decl_loc);
      if (strcmp(result.SourceManager->getBufferName(spell),
                 "<scratch space>") == 0)
        return;
    }
    clang::ASTContext* context = result.Context;
    std::string new_name;
    if (!GetNameForDecl(*decl, *context, new_name))
      return;  // If false, the name was not suitable for renaming.
    llvm::StringRef old_name = decl->getName();
    if (old_name == new_name)
      return;
    clang::SourceLocation loc = TargetNodeTraits<TargetNode>::GetLoc(
        *result.Nodes.getNodeAs<TargetNode>(
            TargetNodeTraits<TargetNode>::kName));
    clang::CharSourceRange range = clang::CharSourceRange::getTokenRange(loc);
    replacements_->emplace(*result.SourceManager, range, new_name);
    replacement_names_.emplace(old_name.str(), std::move(new_name));
  }

  const std::unordered_map<std::string, std::string>& replacement_names()
      const {
    return replacement_names_;
  }

 private:
  Replacements* const replacements_;
  std::unordered_map<std::string, std::string> replacement_names_;
};

using FieldDeclRewriter = RewriterBase<clang::FieldDecl, clang::NamedDecl>;
using VarDeclRewriter = RewriterBase<clang::VarDecl, clang::NamedDecl>;
using MemberRewriter = RewriterBase<clang::FieldDecl, clang::MemberExpr>;
using DeclRefRewriter = RewriterBase<clang::VarDecl, clang::DeclRefExpr>;
using FieldDeclRefRewriter = RewriterBase<clang::FieldDecl, clang::DeclRefExpr>;
using FunctionDeclRewriter =
    RewriterBase<clang::FunctionDecl, clang::NamedDecl>;
using FunctionRefRewriter =
    RewriterBase<clang::FunctionDecl, clang::DeclRefExpr>;
using ConstructorInitializerRewriter =
    RewriterBase<clang::FieldDecl, clang::CXXCtorInitializer>;

using MethodDeclRewriter = RewriterBase<clang::CXXMethodDecl, clang::NamedDecl>;
using MethodRefRewriter =
    RewriterBase<clang::CXXMethodDecl, clang::DeclRefExpr>;
using MethodMemberRewriter =
    RewriterBase<clang::CXXMethodDecl, clang::MemberExpr>;

using EnumConstantDeclRewriter =
    RewriterBase<clang::EnumConstantDecl, clang::NamedDecl>;
using EnumConstantDeclRefRewriter =
    RewriterBase<clang::EnumConstantDecl, clang::DeclRefExpr>;

using UsingDeclRewriter = RewriterBase<clang::UsingDecl, clang::NamedDecl>;

}  // namespace

static llvm::cl::extrahelp common_help(CommonOptionsParser::HelpMessage);

int main(int argc, const char* argv[]) {
  // TODO(dcheng): Clang tooling should do this itself.
  // http://llvm.org/bugs/show_bug.cgi?id=21627
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmParser();
  llvm::cl::OptionCategory category(
      "rewrite_to_chrome_style: convert Blink style to Chrome style.");
  CommonOptionsParser options(argc, argv, category);
  clang::tooling::ClangTool tool(options.getCompilations(),
                                 options.getSourcePathList());

  MatchFinder match_finder;
  Replacements replacements;

  auto in_blink_namespace =
      decl(hasAncestor(namespaceDecl(anyOf(hasName("blink"), hasName("WTF")),
                                     hasParent(translationUnitDecl()))),
           unless(isExpansionInFileMatching(kGeneratedFileRegex)));

  // Field, variable, and enum declarations ========
  // Given
  //   int x;
  //   struct S {
  //     int y;
  //     enum { VALUE };
  //   };
  // matches |x|, |y|, and |VALUE|.
  auto field_decl_matcher = id("decl", fieldDecl(in_blink_namespace));
  auto var_decl_matcher = id("decl", varDecl(in_blink_namespace));
  auto enum_member_decl_matcher =
      id("decl", enumConstantDecl(in_blink_namespace));

  FieldDeclRewriter field_decl_rewriter(&replacements);
  match_finder.addMatcher(field_decl_matcher, &field_decl_rewriter);

  VarDeclRewriter var_decl_rewriter(&replacements);
  match_finder.addMatcher(var_decl_matcher, &var_decl_rewriter);

  EnumConstantDeclRewriter enum_member_decl_rewriter(&replacements);
  match_finder.addMatcher(enum_member_decl_matcher, &enum_member_decl_rewriter);

  // Field, variable, and enum references ========
  // Given
  //   bool x = true;
  //   if (x) {
  //     ...
  //   }
  // matches |x| in if (x).
  auto member_matcher = id(
      "expr",
      memberExpr(
          member(field_decl_matcher),
          // Needed to avoid matching member references in functions (which will
          // be an ancestor of the member reference) synthesized by the
          // compiler, such as a synthesized copy constructor.
          // This skips explicitly defaulted functions as well, but that's OK:
          // there's nothing interesting to rewrite in those either.
          unless(hasAncestor(functionDecl(internal_hack::isDefaulted())))));
  auto decl_ref_matcher = id("expr", declRefExpr(to(var_decl_matcher)));
  auto enum_member_ref_matcher =
      id("expr", declRefExpr(to(enum_member_decl_matcher)));

  MemberRewriter member_rewriter(&replacements);
  match_finder.addMatcher(member_matcher, &member_rewriter);

  DeclRefRewriter decl_ref_rewriter(&replacements);
  match_finder.addMatcher(decl_ref_matcher, &decl_ref_rewriter);

  EnumConstantDeclRefRewriter enum_member_ref_rewriter(&replacements);
  match_finder.addMatcher(enum_member_ref_matcher, &enum_member_ref_rewriter);

  // Member references in a non-member context ========
  // Given
  //   struct S {
  //     typedef int U::*UnspecifiedBoolType;
  //     operator UnspecifiedBoolType() { return s_ ? &U::s_ : 0; }
  //     int s_;
  //   };
  // matches |&U::s_| but not |s_|.
  auto member_ref_matcher = id("expr", declRefExpr(to(field_decl_matcher)));

  FieldDeclRefRewriter member_ref_rewriter(&replacements);
  match_finder.addMatcher(member_ref_matcher, &member_ref_rewriter);

  // Non-method function declarations ========
  // Given
  //   void f();
  //   struct S {
  //     void g();
  //   };
  // matches |f| but not |g|.
  auto function_decl_matcher = id(
      "decl",
      functionDecl(
          unless(anyOf(
              // Methods are covered by the method matchers.
              cxxMethodDecl(),
              // Out-of-line overloaded operators have special names and should
              // never be renamed.
              isOverloadedOperator())),
          in_blink_namespace));
  FunctionDeclRewriter function_decl_rewriter(&replacements);
  match_finder.addMatcher(function_decl_matcher, &function_decl_rewriter);

  // Non-method function references ========
  // Given
  //   f();
  //   void (*p)() = &f;
  // matches |f()| and |&f|.
  auto function_ref_matcher =
      id("expr", declRefExpr(to(function_decl_matcher)));
  FunctionRefRewriter function_ref_rewriter(&replacements);
  match_finder.addMatcher(function_ref_matcher, &function_ref_rewriter);

  // Method declarations ========
  // Given
  //   struct S {
  //     void g();
  //   };
  // matches |g|.
  // For a method to be considered for rewrite, it must not override something
  // that we're not rewriting. Any methods that we would not normally consider
  // but that override something we are rewriting should also be rewritten. So
  // we use includeAllOverriddenMethods() to check these rules not just for the
  // method being matched but for the methods it overrides also.
  auto is_blink_method = includeAllOverriddenMethods(
      allOf(in_blink_namespace, unless(isBlacklistedMethod())));
  auto method_decl_matcher = id(
      "decl",
      cxxMethodDecl(
          unless(anyOf(
              // Overloaded operators have special names and should never be
              // renamed.
              isOverloadedOperator(),
              // Similarly, constructors, destructors, and conversion functions
              // should not be considered for renaming.
              cxxConstructorDecl(), cxxDestructorDecl(), cxxConversionDecl())),
          // Check this last after excluding things, to avoid
          // asserts about overriding non-blink and blink for the
          // same method.
          is_blink_method));
  MethodDeclRewriter method_decl_rewriter(&replacements);
  match_finder.addMatcher(method_decl_matcher, &method_decl_rewriter);

  // Method references in a non-member context ========
  // Given
  //   S s;
  //   s.g();
  //   void (S::*p)() = &S::g;
  // matches |&S::g| but not |s.g()|.
  auto method_ref_matcher = id("expr", declRefExpr(to(method_decl_matcher)));

  MethodRefRewriter method_ref_rewriter(&replacements);
  match_finder.addMatcher(method_ref_matcher, &method_ref_rewriter);

  // Method references in a member context ========
  // Given
  //   S s;
  //   s.g();
  //   void (S::*p)() = &S::g;
  // matches |s.g()| but not |&S::g|.
  auto method_member_matcher =
      id("expr", memberExpr(member(method_decl_matcher)));

  MethodMemberRewriter method_member_rewriter(&replacements);
  match_finder.addMatcher(method_member_matcher, &method_member_rewriter);

  // Initializers ========
  // Given
  //   struct S {
  //     int x;
  //     S() : x(2) {}
  //   };
  // matches each initializer in the constructor for S.
  auto constructor_initializer_matcher =
      cxxConstructorDecl(forEachConstructorInitializer(id(
          "initializer",
          cxxCtorInitializer(forAnyField(field_decl_matcher), isWritten()))));

  ConstructorInitializerRewriter constructor_initializer_rewriter(
      &replacements);
  match_finder.addMatcher(constructor_initializer_matcher,
                          &constructor_initializer_rewriter);

  // Using declarations ========
  // Given
  //   using blink::X;
  // matches |using blink::X|.
  auto function_template_decl_matcher =
      id("decl", functionTemplateDecl(templatedDecl(anyOf(function_decl_matcher,
                                                          method_decl_matcher)),
                                      in_blink_namespace));
  UsingDeclRewriter using_decl_rewriter(&replacements);
  match_finder.addMatcher(
      id("decl",
         usingDecl(hasAnyUsingShadowDecl(hasTargetDecl(
             anyOf(var_decl_matcher, field_decl_matcher, function_decl_matcher,
                   method_decl_matcher, function_template_decl_matcher,
                   enum_member_decl_matcher))))),
      &using_decl_rewriter);

  std::unique_ptr<clang::tooling::FrontendActionFactory> factory =
      clang::tooling::newFrontendActionFactory(&match_finder);
  int result = tool.run(factory.get());
  if (result != 0)
    return result;

#if defined(_WIN32)
  HANDLE lockfd = CreateFile("rewrite-sym.lock", GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  OVERLAPPED overlapped = {};
  LockFileEx(lockfd, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &overlapped);
#else
  int lockfd = open("rewrite-sym.lock", O_RDWR | O_CREAT, 0666);
  while (flock(lockfd, LOCK_EX)) {  // :D
  }
#endif

  std::ofstream replacement_db_file("rewrite-sym.txt",
                                    std::ios_base::out | std::ios_base::app);
  for (const auto& p : field_decl_rewriter.replacement_names())
    replacement_db_file << "var:" << p.first << ":" << p.second << "\n";
  for (const auto& p : var_decl_rewriter.replacement_names())
    replacement_db_file << "var:" << p.first << ":" << p.second << "\n";
  for (const auto& p : enum_member_decl_rewriter.replacement_names())
    replacement_db_file << "enu:" << p.first << ":" << p.second << "\n";
  for (const auto& p : function_decl_rewriter.replacement_names())
    replacement_db_file << "fun:" << p.first << ":" << p.second << "\n";
  for (const auto& p : method_decl_rewriter.replacement_names())
    replacement_db_file << "fun:" << p.first << ":" << p.second << "\n";
  replacement_db_file.close();

#if defined(_WIN32)
  UnlockFileEx(lockfd, 0, 1, 0, &overlapped);
  CloseHandle(lockfd);
#else
  flock(lockfd, LOCK_UN);
  close(lockfd);
#endif

  // Serialization format is documented in tools/clang/scripts/run_tool.py
  llvm::outs() << "==== BEGIN EDITS ====\n";
  for (const auto& r : replacements) {
    std::string replacement_text = r.getReplacementText().str();
    std::replace(replacement_text.begin(), replacement_text.end(), '\n', '\0');
    llvm::outs() << "r:::" << r.getFilePath() << ":::" << r.getOffset()
                 << ":::" << r.getLength() << ":::" << replacement_text << "\n";
  }
  llvm::outs() << "==== END EDITS ====\n";

  return 0;
}
