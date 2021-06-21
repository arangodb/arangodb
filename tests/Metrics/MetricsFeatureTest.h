////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef METRICS_FEATURE_TEST_H
#define METRICS_FEATURE_TEST_H
#include <RestServer/MetricsFeature.h>
struct HISTOGRAMLOGSCALE {
  static log_scale_t<double> scale() { return { 10.0, 0, 1000.0, 8 }; }
};
struct HISTOGRAMLINSCALE {
  static lin_scale_t<double> scale() { return { 0., 1., 10 }; }
};
struct COUNTER : arangodb::metrics::CounterBuilder<COUNTER> {
  COUNTER() { _name = "COUNTER", _help = "one counter"; }
};
struct HISTOGRAMLOG : arangodb::metrics::HistogramBuilder<HISTOGRAMLOG, HISTOGRAMLOGSCALE> {
  HISTOGRAMLOG() { _name = "HISTOGRAMLOG", _help = "one histogram with log scale"; }
};
struct HISTOGRAMLIN : arangodb::metrics::HistogramBuilder<HISTOGRAMLIN, HISTOGRAMLINSCALE> {
  HISTOGRAMLIN() { _name = "HISTOGRAMLIN", _help = "one histogram with lin scale"; }
};
struct GAUGE : arangodb::metrics::GaugeBuilder<GAUGE, uint64_t> {
  GAUGE() { _name = "GAUGE", _help = "one gauge"; }
};
#endif
