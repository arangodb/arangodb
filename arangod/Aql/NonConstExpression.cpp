#include "NonConstExpression.h"

using namespace arangodb::aql;

NonConstExpression::NonConstExpression(std::unique_ptr<Expression> exp,
                                       std::vector<size_t> idxPath)
    : expression(std::move(exp)), indexPath(std::move(idxPath)) {}
