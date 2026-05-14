#include "clang/ASTMatchers/ASTMatchFinder.h"

namespace matcher {

auto match(clang::ast_matchers::MatchFinder::MatchCallback& callback)
    -> clang ::ast_matchers::MatchFinder;

}
