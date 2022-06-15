////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "InsertDocuments.h"

#include <fstream>
#include <memory>
#include <stdexcept>

#include "Basics/overload.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "velocypack/Builder.h"
#include "VocBase/Methods/Collections.h"

#include "Execution.h"
#include "Server.h"
#include "ValueGenerators/RandomStringGenerator.h"

namespace arangodb::sepp::workloads {

auto InsertDocuments::stoppingCriterion() const noexcept
    -> StoppingCriterion::type {
  return _options.stop;
}

auto InsertDocuments::createThreads(Execution& exec, Server& server)
    -> WorkerThreadList {
  ThreadOptions defaultThread;
  defaultThread.stop = _options.stop;

  if (_options.defaultThreadOptions) {
    auto& defaultOpts = _options.defaultThreadOptions.value();
    defaultThread.collection = defaultOpts.collection;
    defaultThread.documentsPerTrx = defaultOpts.documentsPerTrx;
    defaultThread.documentModifier = defaultOpts.documentModifier;
    if (std::holds_alternative<std::string>(defaultOpts.document)) {
      auto documentFile = std::get<std::string>(defaultOpts.document);
      try {
        std::ifstream t(documentFile);
        std::stringstream buffer;
        buffer << t.rdbuf();

        defaultThread.document =
            arangodb::velocypack::Parser::fromJson(buffer.str());
      } catch (std::exception const& e) {
        throw std::runtime_error("Failed to parse object file '" +
                                 documentFile + "' - " + e.what());
      }
    } else {
      defaultThread.document = std::make_shared<velocypack::Builder>();
      defaultThread.document->add(
          std::get<velocypack::Slice>(defaultOpts.document));
    }
  }

  WorkerThreadList result;
  for (std::uint32_t i = 0; i < _options.threads; ++i) {
    result.emplace_back(std::make_unique<Thread>(defaultThread, exec, server));
  }
  return result;
}

InsertDocuments::Thread::Thread(ThreadOptions options, Execution& exec,
                                Server& server)
    : ExecutionThread(exec, server),
      _options(std::move(options)),
      _modifier(_options.documentModifier) {}

InsertDocuments::Thread::~Thread() = default;

void InsertDocuments::Thread::run() {
  auto trx = std::make_unique<SingleCollectionTransaction>(
      transaction::StandaloneContext::Create(*_server.vocbase()),
      _options.collection, AccessMode::Type::WRITE);

  auto res = trx->begin();
  if (!res.ok()) {
    throw std::runtime_error("Failed to begin trx: " +
                             std::string(res.errorMessage()));
  }

  velocypack::Builder builder;
  for (std::uint32_t j = 0; j < _options.documentsPerTrx; ++j) {
    buildDocument(builder);
    auto res = trx->insert(_options.collection, builder.slice(), {});
    if (!res.ok()) {
      throw std::runtime_error("Failed to insert document in trx: " +
                               std::string(res.errorMessage()));
    }
  }

  res = trx->commit();
  if (!res.ok()) {
    throw std::runtime_error("Failed to commit trx: " +
                             std::string(res.errorMessage()));
  }
  _operations += _options.documentsPerTrx;
}

void InsertDocuments::Thread::buildDocument(velocypack::Builder& builder) {
  builder.clear();
  builder.openObject();
  for (auto [k, v] : VPackObjectIterator(_options.document->slice())) {
    builder.add(k);
    builder.add(v);
  }
  _modifier.apply(builder);
  builder.close();
}

auto InsertDocuments::Thread::shouldStop() const noexcept -> bool {
  if (execution().stopped()) {
    return true;
  }

  using StopAfterOps = StoppingCriterion::NumberOfOperations;
  if (std::holds_alternative<StopAfterOps>(_options.stop)) {
    return _operations >= std::get<StopAfterOps>(_options.stop).count;
  }
  return false;
}

InsertDocuments::Thread::DocumentModifier::DocumentModifier(
    std::unordered_map<std::string, Options::DocumentModifier> const&
        modifiers) {
  for (auto& [attr, mod] : modifiers) {
    _generators.emplace(
        attr,
        std::visit(overload{[](Options::RandomStringGenerator const& g) {
                     return std::make_unique<generators::RandomStringGenerator>(
                         g.size);
                   }},
                   mod.value));
  }
}

void InsertDocuments::Thread::DocumentModifier::apply(
    velocypack::Builder& builder) {
  for (auto& [attr, gen] : _generators) {
    builder.add(VPackValue(attr));
    gen->apply(builder);
  }
}

}  // namespace arangodb::sepp::workloads
