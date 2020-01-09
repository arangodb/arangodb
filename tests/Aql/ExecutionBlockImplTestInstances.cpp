#include "Aql/ExecutionBlockImpl.cpp"
#include "TestEmptyExecutorHelper.h"
#include "TestExecutorHelper.h"
#include "TestLambdaExecutor.h"

template class ::arangodb::aql::ExecutionBlockImpl<TestExecutorHelper>;
template class ::arangodb::aql::ExecutionBlockImpl<TestEmptyExecutorHelper>;
template class ::arangodb::aql::ExecutionBlockImpl<TestLambdaExecutor<BlockPassthrough::Enable>>;
template class ::arangodb::aql::ExecutionBlockImpl<TestLambdaExecutor<BlockPassthrough::Disable>>;
template class ::arangodb::aql::ExecutionBlockImpl<TestLambdaSkipExecutor>;
