#include "Aql/ExecutionBlockImpl.cpp"
#include "TestEmptyExecutorHelper.h"
#include "TestExecutorHelper.h"

template class ::arangodb::aql::ExecutionBlockImpl<TestExecutorHelper>;
template class ::arangodb::aql::ExecutionBlockImpl<TestEmptyExecutorHelper>;
