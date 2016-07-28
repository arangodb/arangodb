////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "AffinityFeature.h"

#include "Dispatcher/DispatcherFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8Server/V8DealerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

AffinityFeature::AffinityFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Affinity"),
      _threadAffinity(0),
      _n(0),
      _nd(0),
      _ns(0) {
  startsAfter("Logger");
  startsAfter("Dispatcher");
  startsAfter("Scheduler");
}

void AffinityFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  std::unordered_set<uint32_t> choices{0, 1, 2, 3, 4};

  options->addHiddenOption(
      "--server.thread-affinity",
      "set thread affinity (0=disable, 1=disjunct, 2=overlap, 3=scheduler, "
      "4=dispatcher)",
      new DiscreteValuesParameter<UInt32Parameter>(&_threadAffinity, choices));
}

void AffinityFeature::prepare() {
  _n = TRI_numberProcessors();

  if (_n <= 2 || _threadAffinity == 0) {
    return;
  }

#if !(defined(ARANGODB_HAVE_THREAD_AFFINITY) || \
      defined(ARANGODB_HAVE_THREAD_POLICY))

  LOG(WARN) << "thread affinity is not supported on this operating system";
  _threadAffinity = 0;

#else

  DispatcherFeature* dispatcher =
      ApplicationServer::getFeature<DispatcherFeature>("Dispatcher");

  _nd = (dispatcher != nullptr) ? dispatcher->concurrency() : 0;

  SchedulerFeature* scheduler =
      ApplicationServer::getFeature<SchedulerFeature>("Scheduler");

  _ns = (scheduler != nullptr) ? scheduler->concurrency() : 0;

  if (_ns == 0 && _nd == 0) {
    return;
  }

  switch (_threadAffinity) {
    case 1:
      if (_n < _ns + _nd) {
        _ns = static_cast<size_t>(round(1.0 * _n * _ns / (_ns + _nd)));
        _nd = static_cast<size_t>(round(1.0 * _n * _nd / (_ns + _nd)));

        if (_ns < 1) {
          _ns = 1;
        }
        if (_nd < 1) {
          _nd = 1;
        }

        while (_n < _ns + _nd) {
          if (1 < _ns) {
            _ns -= 1;
          } else if (1 < _nd) {
            _nd -= 1;
          } else {
            _ns = 1;
            _nd = 1;
          }
        }
      }

      break;

    case 2:
      if (_n < _ns) {
        _ns = _n;
      }

      if (_n < _nd) {
        _nd = _n;
      }

      break;

    case 3:
      if (_n < _ns) {
        _ns = _n;
      }

      _nd = 0;

      break;

    case 4:
      if (_n < _nd) {
        _nd = _n;
      }

      _ns = 0;

      break;

    default:
      _threadAffinity = 0;
      break;
  }

  if (0 < _threadAffinity) {
    TRI_ASSERT(_ns <= _n);
    TRI_ASSERT(_nd <= _n);

    for (size_t i = 0; i < _ns; ++i) {
      _ps.push_back(i);
    }

    for (size_t i = 0; i < _nd; ++i) {
      _pd.push_back(_n - i - 1);
    }

    if (0 < _ns && scheduler != nullptr) {
      scheduler->setProcessorAffinity(_ps);
    }

    if (0 < _nd && dispatcher != nullptr) {
      dispatcher->setProcessorAffinity(_pd);
    }
  }

#endif
}

void AffinityFeature::start() {
  if (0 < _threadAffinity) {
    LOG(INFO) << "the server has " << _n << " (hyper) cores, using " << _ns
              << " scheduler thread(s), " << _nd << " dispatcher thread(s)";
  } else {
    DispatcherFeature* dispatcher =
        ApplicationServer::getFeature<DispatcherFeature>("Dispatcher");
    SchedulerFeature* scheduler =
        ApplicationServer::getFeature<SchedulerFeature>("Scheduler");

    size_t nd = (dispatcher == nullptr ? 0 : dispatcher->concurrency());
    size_t ns = (scheduler == nullptr ? 0 : scheduler->concurrency());

    LOG(INFO) << "the server has " << _n << " (hyper) cores, using " << ns
              << " scheduler thread(s), " << nd << " dispatcher thread(s)";
  }
}
