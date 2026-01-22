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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Context.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ClusterInfo.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>

using namespace arangodb;

namespace {
// custom type value handler, used for deciphering the _id attribute
struct CustomTypeHandler final : public VPackCustomTypeHandler {
  CustomTypeHandler(TRI_vocbase_t& vocbase,
                    CollectionNameResolver const& resolver)
      : vocbase(vocbase), resolver(resolver) {}

  ~CustomTypeHandler() = default;

  void dump(VPackSlice const& value, VPackDumper* dumper,
            VPackSlice const& base) override final {
    dumper->appendString(toString(value, nullptr, base));
  }

  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    return transaction::helpers::extractIdString(&resolver, value, base);
  }

  TRI_vocbase_t& vocbase;
  CollectionNameResolver const& resolver;
};
}  // namespace

/// @brief create the context
transaction::Context::Context(TRI_vocbase_t& vocbase,
                              OperationOrigin operationOrigin)
    : _vocbase(vocbase),
      _resolver(std::make_unique<CollectionNameResolver>(_vocbase)),
      _customTypeHandler(createCustomTypeHandler(_vocbase, *_resolver)),
      _options(velocypack::Options::Defaults),
      _operationOrigin(operationOrigin) {
  _options.customTypeHandler = _customTypeHandler.get();
}

/// @brief destroy the context
transaction::Context::~Context() {
  // call the actual cleanup routine which frees all
  // hogged resources
  cleanup();
}

/// @brief destroys objects owned by the context,
/// this can be called multiple times.
/// currently called by dtor and by unit test mocks.
/// we cannot move this into the dtor (where it was before) because
/// the mocked objects in unittests do not seem to call it and effectively leak.
void transaction::Context::cleanup() noexcept { _resolver.reset(); }

/// @brief factory to create a custom type handler, not managed
std::unique_ptr<VPackCustomTypeHandler>
transaction::Context::createCustomTypeHandler(
    TRI_vocbase_t& vocbase, CollectionNameResolver const& resolver) {
  return std::make_unique<::CustomTypeHandler>(vocbase, resolver);
}

/// @brief get velocypack options with a custom type handler
VPackOptions const* transaction::Context::getVPackOptions() const noexcept {
  return &_options;
}

velocypack::CustomTypeHandler* transaction::Context::getCustomTypeHandler()
    const noexcept {
  TRI_ASSERT(_customTypeHandler != nullptr);
  return _customTypeHandler.get();
}

/// @brief create a resolver
CollectionNameResolver const& transaction::Context::resolver() const noexcept {
  TRI_ASSERT(_resolver != nullptr);
  return *_resolver;
}

std::shared_ptr<TransactionState> transaction::Context::createState(
    transaction::Options const& options) {
  // now start our own transaction
  return vocbase().engine().createTransactionState(_vocbase, generateId(),
                                                   options, _operationOrigin);
}

TransactionId transaction::Context::generateId() const {
  return Context::makeTransactionId();
}

std::shared_ptr<transaction::Context> transaction::Context::clone() const {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      "transaction::Context::clone() is not implemented");
}

void transaction::Context::setCounterGuard(
    std::shared_ptr<transaction::CounterGuard> guard) noexcept {
  _counterGuard = std::move(guard);
}

/*static*/ TransactionId transaction::Context::makeTransactionId() {
  auto role = ServerState::instance()->getRole();
  if (ServerState::isCoordinator(role)) {
    return TransactionId::createCoordinator();
  } else if (ServerState::isDBServer(role)) {
    return TransactionId::createLegacy();
  }
  return TransactionId::createSingleServer();
}
