////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Ast/ParseModificationOptions.h"
#include "Aql/AstNode.h"
#include "Aql/TypedAstNodes.h"
#include "Basics/StaticStrings.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

#include <string_view>

namespace arangodb::aql::ast {
/// @brief create modification options from an AST node
auto parseModificationOptions(std::string_view operationName,
                              AstNode const* node)
    -> ParseModificationOptionsResult {
  TRI_ASSERT(operationName == "INSERT" || operationName == "UPDATE" ||
             operationName == "REPLACE" || operationName == "REMOVE" ||
             operationName == "UPSERT");

  ParseModificationOptionsResult result;

  // parse the modification options we got
  if (node != nullptr && node->type == NODE_TYPE_OBJECT) {
    size_t n = node->numMembers();

    for (size_t i = 0; i < n; ++i) {
      auto member = node->getMemberUnchecked(i);

      if (member != nullptr && member->type == NODE_TYPE_OBJECT_ELEMENT) {
        auto const name = member->getStringView();
        ast::ObjectElementNode objElem(member);
        auto value = objElem.getValue();

        TRI_ASSERT(value->isConstant());

        if (name == StaticStrings::WaitForSyncString) {
          result.options.waitForSync = value->isTrue();
        } else if (name == StaticStrings::SkipDocumentValidation) {
          result.options.validate = !value->isTrue();
        } else if (name == StaticStrings::KeepNullString) {
          result.options.keepNull = value->isTrue();
        } else if (name == StaticStrings::RefillIndexCachesString) {
          result.options.refillIndexCaches =
              value->isTrue() ? RefillIndexCaches::kRefill
                              : RefillIndexCaches::kDontRefill;
        } else if (name == StaticStrings::MergeObjectsString) {
          result.options.mergeObjects = value->isTrue();
        } else if (name == "overwrite" && value->isTrue()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "the 'overwrite' option has been removed, use "
              "'overwriteMode' instead");
        } else if (name == StaticStrings::OverwriteMode &&
                   value->isStringValue()) {
          auto overwriteMode =
              OperationOptions::determineOverwriteMode(value->getStringView());

          if (overwriteMode != OperationOptions::OverwriteMode::Unknown) {
            result.options.overwriteMode = overwriteMode;
            // Set useOldSmartGraphVariable to true when overwriteMode is
            // "ignore"
            if (overwriteMode == OperationOptions::OverwriteMode::Ignore) {
              result.options.useOldSmartGraphVariable = true;
            }
          }
        } else if (name == StaticStrings::IgnoreRevsString) {
          result.options.ignoreRevs = value->isTrue();
        } else if (name == StaticStrings::VersionAttributeString &&
                   value->isStringValue() &&
                   (operationName == "INSERT" || operationName == "UPDATE" ||
                    operationName == "REPLACE")) {
          result.options.versionAttribute = value->getString();
        } else if (name == "exclusive") {
          result.options.exclusive = value->isTrue();
        } else if (name == "ignoreErrors") {
          result.options.ignoreErrors = value->isTrue();
        } else if (name == StaticStrings::ReadOwnWrites &&
                   operationName == "UPSERT") {
          // UPSERT supports "readOwnWrites" attribute, but other
          // modification options don't.
          // the value of readOwnWrites is already picked up during AST creation
          // and does not need to be handled here anymore.
        } else {
          result.warnings.emplace_back(
              std::tuple<std::string, std::string, std::string>{
                  "unknown", operationName, name});
        }
      }
    }
  }

  return result;
}

}  // namespace arangodb::aql::ast
