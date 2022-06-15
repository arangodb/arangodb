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

#pragma once

#include <optional>
#include <unordered_map>
#include <variant>

#include "Inspection/Status.h"
#include "Inspection/Types.h"
#include "ValueGenerators/RandomStringGenerator.h"
#include "velocypack/SliceContainer.h"

#include "ExecutionThread.h"
#include "StoppingCriterion.h"
#include "ValueGenerator.h"
#include "Workload.h"

namespace arangodb::sepp::workloads {

struct InsertDocuments : Workload {
  struct ThreadOptions;
  struct Options;
  struct Thread;

  InsertDocuments(Options const& options) : _options(options) {}

  auto createThreads(Execution& exec, Server& server)
      -> WorkerThreadList override;
  auto stoppingCriterion() const noexcept -> StoppingCriterion::type override;

 private:
  Options const& _options;
};

struct InsertDocuments::Options {
  struct Document : std::variant<std::string, velocypack::Slice> {
    template<class Inspector>
    friend inline auto inspect(Inspector& f, Document& o) {
      namespace insp = arangodb::inspection;
      return f.variant(o)
          .qualified("source", "value")
          .alternatives(insp::type<std::string>("file"),
                        insp::type<velocypack::Slice>("inline"));
    }
  };

  struct RandomStringGenerator {
    std::uint32_t size;
    template<class Inspector>
    friend inline auto inspect(Inspector& f, RandomStringGenerator& o) {
      return f.object(o).fields(f.field("size", o.size));
    }
  };

  struct DocumentModifier {
    template<class Inspector>
    friend inline auto inspect(Inspector& f, DocumentModifier& o) {
      namespace insp = arangodb::inspection;
      return f.variant(o.value).unqualified().alternatives(
          insp::type<RandomStringGenerator>("randomString"));
    }

    std::variant<RandomStringGenerator> value;
  };

  struct Thread {
    std::string collection;
    std::uint32_t documentsPerTrx;
    Document document;
    std::unordered_map<std::string, DocumentModifier> documentModifier;

    template<class Inspector>
    friend inline auto inspect(Inspector& f, Thread& o) {
      return f.object(o).fields(
          f.field("document", o.document),
          f.field("documentModifier", o.documentModifier).fallback(f.keep()),
          f.field("documentsPerTrx", o.documentsPerTrx).fallback(1u),
          f.field("collection", o.collection));
    }
  };

  std::optional<Thread> defaultThreadOptions;
  std::uint32_t threads{1};  // TODO - make variant fixed number/array of Thread
  StoppingCriterion::type stop;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, Options& o) {
    return f.object(o).fields(f.field("default", o.defaultThreadOptions),
                              f.field("threads", o.threads),
                              f.field("stopAfter", o.stop));
  }
};

struct InsertDocuments::ThreadOptions {
  std::string collection;
  std::uint32_t documentsPerTrx{1};
  std::shared_ptr<velocypack::Builder> document;
  std::unordered_map<std::string, Options::DocumentModifier> documentModifier;
  StoppingCriterion::type stop;
};

struct InsertDocuments::Thread : ExecutionThread {
  Thread(ThreadOptions options, Execution& exec, Server& server);
  ~Thread();

  void run() override;
  [[nodiscard]] auto report() const -> ThreadReport override {
    return {.data = {}, .operations = _operations};
  }
  auto shouldStop() const noexcept -> bool override;

  struct DocumentModifier {
    explicit DocumentModifier(
        std::unordered_map<std::string, Options::DocumentModifier> const&);
    void apply(velocypack::Builder&);

   private:
    std::unordered_map<std::string, std::unique_ptr<ValueGenerator>>
        _generators;
  };

 private:
  void buildDocument(velocypack::Builder&);
  ThreadOptions _options;
  DocumentModifier _modifier;
  std::uint64_t _operations{0};
};

}  // namespace arangodb::sepp::workloads