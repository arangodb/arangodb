# This code is shared with arangosh
add_library(arangoimport_utils OBJECT
  AutoTuneThread.cpp
  ImportHelper.cpp
  SenderThread.cpp)
target_include_directories(arangoimport_utils PRIVATE ${PROJECT_SOURCE_DIR}/client-tools)
target_link_libraries(arangoimport_utils
    arango_basic_utils
    arango_basic_errors
    arango_basic_logger
)

################################################################################
## AQL Standalone Expression Library
##
## A subset of arango_aql compiled for standalone expression evaluation in
## client tools (no cluster, no storage engine, no full server context).
## Used by the --custom-query feature of arangoimport.
##
## This follows the StandaloneCalculation / ComputedValues pattern from arangod.
## Any server symbols that are referenced by headers but never called at
## runtime are provided by AqlStandaloneStubs.cpp.
################################################################################

set(AQL_STANDALONE_SOURCES
  # Core infrastructure
  ${PROJECT_SOURCE_DIR}/arangod/Aql/StandaloneCalculation.cpp
  # QueryContext.cpp is NOT compiled here — its constructor/destructor call
  # getFeature<DatabaseFeature>() which is unavailable in client tools.
  # A stub is provided in AqlStandaloneStubs.cpp instead.
  ${PROJECT_SOURCE_DIR}/arangod/Aql/QueryString.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/QueryOptions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/QueryWarnings.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/BindParameters.cpp
  # Parser
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Parser.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/grammar.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/tokens.cpp
  # AST
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Ast.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AstNode.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AstResources.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AstHelper.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/LazyConditions.cpp
  # Expression evaluation
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Expression.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/NonConstExpression.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/NonConstExpressionContainer.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AttributeAccessor.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/FixedVarExpressionContext.cpp
  # Variables and scopes
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Variable.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/VariableGenerator.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Scopes.cpp
  # Values
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AqlValue.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AqlValueGroup.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AqlValueMaterializer.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Arithmetic.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Range.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Quantifier.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/ShortStringStorage.cpp
  # Functions
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Functions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AqlFunctionFeature.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AqlFunctionsInternalCache.cpp
  # Function implementations (all needed for the function registry)
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/ArrayFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/BitFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/DateFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/DecayFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/EncodingFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/HashFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/IpFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/JsonFunctions.cpp
  # LevenshteinFunctions.cpp excluded — depends on IResearch headers
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/MathFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/MiscFunctions.cpp
  # NGramFunctions.cpp excluded — depends on IResearch headers
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/ObjectFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/ParseFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/RandomFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/RegexFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/StringFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/StatisticsFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/TypeCheckFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/UuidFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/VectorFunctions.cpp
  # Functions that reference server infrastructure (may need stubs):
  # CollectionFunctions.cpp excluded — depends on Cluster headers
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/DistanceFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/DistributeFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/DocumentFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/GeoFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/InformationFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/InternalFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/KeyFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/RevisionFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/SchemaFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/SystemFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/TestFunctions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Function/CallApplyFunctions.cpp
  # AQL transaction (used by StandaloneCalculation/ImportQueryContext)
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AqlTransaction.cpp
  # Supporting types referenced by Ast/Expression
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Aggregator.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/ModificationOptions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Collection.cpp
  # Collections.cpp is NOT compiled here — its add() method calls
  # getFeature<QueryRegistryFeature>() which is unavailable in client tools.
  # A stub is provided in AqlStandaloneStubs.cpp instead.
  ${PROJECT_SOURCE_DIR}/arangod/Aql/CollectOptions.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Projections.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/SortCondition.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/SortElement.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/AttributeNamePath.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/IndexHint.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Condition.cpp
  # Optimizer (referenced by StandaloneCalculation validateQuery)
  ${PROJECT_SOURCE_DIR}/arangod/Aql/Optimizer.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/OptimizerRules.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/OptimizerRulesFeature.cpp
  ${PROJECT_SOURCE_DIR}/arangod/Aql/OptimizerUtils.cpp
  # Stubs for server symbols not available in client tools
  AqlStandaloneStubs.cpp
  # The custom query transformer itself
  AqlDocumentTransformer.cpp
  ImportExpressionContext.cpp
  AqlStandaloneDummySymbols.c
)

add_library(arango_aql_standalone STATIC ${AQL_STANDALONE_SOURCES})

# grammar.cpp is generated and has an unused variable warning
set_source_files_properties(
  ${PROJECT_SOURCE_DIR}/arangod/Aql/grammar.cpp
  PROPERTIES COMPILE_FLAGS -Wno-unused-but-set-variable
)

get_target_property(IRESEARCH_INCLUDE
  iresearch-static INCLUDE_DIRECTORIES)

target_include_directories(arango_aql_standalone PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}"
  "${PROJECT_SOURCE_DIR}/client-tools"
  "${PROJECT_SOURCE_DIR}/3rdParty/taocpp-json/include"
  "${PROJECT_SOURCE_DIR}/3rdParty/json-schema-validation/include"
  ${IRESEARCH_INCLUDE}
  "${PROJECT_SOURCE_DIR}/3rdParty/rocksdb/include"
  "${PROJECT_SOURCE_DIR}/lib/Actor/include"
  "${PROJECT_SOURCE_DIR}/3rdParty/date/include"
)

target_link_libraries(arango_aql_standalone
  arango_basic_utils
  arango_basic_errors
  arango_basic_logger
  immer
)

################################################################################

add_executable(${BIN_ARANGOIMPORT}
  ${ProductVersionFiles_arangoimport}
  ImportFeature.cpp
  arangoimport.cpp
)
target_include_directories(${BIN_ARANGOIMPORT} PRIVATE
  ${PROJECT_SOURCE_DIR}/client-tools
  ${PROJECT_SOURCE_DIR}/arangod
)

target_link_libraries(${BIN_ARANGOIMPORT}
  ${SYSTEM_LIBRARIES}
  boost_system
  boost_boost
  arango_shell
  arangoimport_utils
  arango_aql_standalone
  clienttools_utils
)
if (USE_JEMALLOC)
  target_link_libraries(${BIN_ARANGOIMPORT} jemalloc)
endif ()

install(
  TARGETS ${BIN_ARANGOIMPORT}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install_config(${BIN_ARANGOIMPORT})

add_dependencies(${BIN_ARANGOIMPORT} zlibstatic)

# use ICU 64 for the client tools
add_dependencies(${BIN_ARANGOIMPORT} iculegacy_build)

add_dependencies(${BIN_ARANGOIMPORT} man)
