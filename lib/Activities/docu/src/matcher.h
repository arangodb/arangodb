#pragma once

#include "clang/ASTMatchers/ASTMatchFinder.h"

namespace matcher {

auto activity_as_field(std::string_view activity_binding)
    -> clang::ast_matchers::internal::BindableMatcher<clang::Decl>;

auto activity_as_variable(std::string_view activity_binding)
    -> clang::ast_matchers::internal::BindableMatcher<clang::Decl>;

}  // namespace matcher
