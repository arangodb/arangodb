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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestHandler/RestVocbaseBaseHandler.h"
#include "Transaction/Methods.h"
#include "VocBase/Methods/Collections.h"

namespace arangodb {

class LogicalCollection;

class RestCollectionHandler : public arangodb::RestVocbaseBaseHandler {
 public:
  RestCollectionHandler(ArangodServer&, GeneralRequest*, GeneralResponse*);

  char const* name() const override final { return "RestCollectionHandler"; }
  RequestLane lane() const override final;

  RestStatus execute() override final;
  void shutdownExecute(bool isFinalized) noexcept override final;

 protected:
  enum class FiguresType { None = 0, Standard = 1, Detailed = 2 };

  enum class CountType { None = 0, Standard = 1, Detailed = 2 };

  void collectionRepresentation(std::string const& name, bool showProperties,
                                FiguresType showFigures, CountType showCount);

  void collectionRepresentation(std::shared_ptr<LogicalCollection> coll,
                                bool showProperties, FiguresType showFigures,
                                CountType showCount);

  void collectionRepresentation(methods::Collections::Context& ctxt,
                                bool showProperties, FiguresType showFigures,
                                CountType showCount);

  futures::Future<futures::Unit> collectionRepresentationAsync(
      methods::Collections::Context& ctxt, bool showProperties,
      FiguresType showFigures, CountType showCount);

  virtual futures::Future<Result> handleExtraCommandPut(
      std::shared_ptr<LogicalCollection> coll, std::string const& command,
      velocypack::Builder& builder) = 0;

 private:
  RestStatus standardResponse();
  futures::Future<futures::Unit> initializeTransaction(LogicalCollection&);

  futures::Future<RestStatus> handleCommandGet();
  void handleCommandPost();
  futures::Future<RestStatus> handleCommandPut();
  void handleCommandDelete();

  VPackBuilder _builder;
  std::unique_ptr<transaction::Methods> _activeTrx;
  std::unique_ptr<methods::Collections::Context> _ctxt;
};

}  // namespace arangodb
