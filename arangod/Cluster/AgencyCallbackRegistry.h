////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Agency/AgencyComm.h"
#include "Agency/AgencyCommon.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Futures/Future.h"
#include "Metrics/Fwd.h"

#include <memory>

namespace arangodb {
class AgencyCallback;

namespace application_features {
class ApplicationServer;
}

class AgencyCallbackRegistry {
 public:
  explicit AgencyCallbackRegistry(ArangodServer&,
                                  std::string const& callbackBasePath);

  ~AgencyCallbackRegistry();

  /// @brief register a callback
  [[nodiscard]] Result registerCallback(std::shared_ptr<AgencyCallback> cb,
                                        bool local = true);

  /// @brief unregister a callback
  bool unregisterCallback(std::shared_ptr<AgencyCallback> cb);

  /// @brief get a callback by its key
  std::shared_ptr<AgencyCallback> getCallback(uint64_t id);

  /// waits for the predicate to return true and resolves the future.
  template<typename F,
           std::enable_if_t<std::is_invocable_r_v<bool, F, velocypack::Slice>,
                            int> = 0>
  auto waitFor(std::string path, F&& fn) -> futures::Future<consensus::index_t>;

  /// observes the given path and invokes the callback. The callback is expected
  /// to return a value that satisfies the std::optional api. If the returned
  /// evaluates to true, the promise is resolved with .value();
  template<typename F,
           typename R =
               std::invoke_result_t<F, velocypack::Slice, consensus::index_t>,
           typename V = typename R::value_type>
  auto waitFor(std::string path, F&& fn) -> futures::Future<V>;

 private:
  std::string getEndpointUrl(uint64_t id) const;

  AgencyComm _agency;

  arangodb::basics::ReadWriteLock _lock;

  std::string const _callbackBasePath;

  std::unordered_map<uint64_t, std::shared_ptr<AgencyCallback>> _callbacks;

  /// @brief total number of callbacks ever registered
  metrics::Counter& _totalCallbacksRegistered;

  /// @brief current number of callbacks registered
  metrics::Gauge<uint64_t>& _callbacksCount;
};

template<
    typename F,
    std::enable_if_t<std::is_invocable_r_v<bool, F, velocypack::Slice>, int>>
auto AgencyCallbackRegistry::waitFor(std::string path, F&& fn)
    -> futures::Future<consensus::index_t> {
  return waitFor(
      std::move(path),
      [fn = std::forward<F>(fn)](
          velocypack::Slice slice,
          consensus::index_t index) -> std::optional<consensus::index_t> {
        if (fn(slice)) {
          return {index};
        }
        return std::nullopt;
      });
}

template<typename F, typename R, typename V>
auto AgencyCallbackRegistry::waitFor(std::string path, F&& fn)
    -> futures::Future<V> {
  using Func = std::decay_t<F>;
  struct Context : Func {
    explicit Context(Func&& fn) : Func(std::move(fn)) {}
    futures::Promise<V> promise;
  };

  auto ctx = std::make_shared<Context>(std::forward<F>(fn));
  auto f = ctx->promise.getFuture();

  auto cb = std::make_shared<AgencyCallback>(
      _agency.server(), std::move(path),
      [ctx](velocypack::Slice slice, consensus::index_t index) -> bool {
        auto pred = ctx->operator()(slice, index);
        if (pred) {
          ctx->promise.setValue(std::move(pred.value()));
          return true;
        }
        return false;
      },
      true, true);

  if (auto result = registerCallback(cb, true); result.fail()) {
    THROW_ARANGO_EXCEPTION(result);
  }

  return std::move(f).then([this, cb](auto&& result) {
    unregisterCallback(cb);
    return std::move(result.get());
  });
}
}  // namespace arangodb
