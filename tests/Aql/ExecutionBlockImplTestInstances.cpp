#include "Aql/ExecutionBlockImpl.cpp"
#include "TestEmptyExecutorHelper.h"
#include "TestLambdaExecutor.h"

template class ::arangodb::aql::ExecutionBlockImpl<TestLambdaExecutor>;
template class ::arangodb::aql::ExecutionBlockImpl<TestLambdaSkipExecutor>;
