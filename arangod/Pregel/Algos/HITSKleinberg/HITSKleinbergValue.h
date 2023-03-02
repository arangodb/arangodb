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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb::pregel::algos {
/// Value for Hyperlink-Induced Topic Search (HITS; also known as
/// hubs and authorities) according to the paper
/// J. Kleinberg, Authoritative sources in a hyperlinked environment,
/// Journal of the ACM. 46 (5): 604–632, 1999,
/// http://www.cs.cornell.edu/home/kleinber/auth.pdf.
struct HITSKleinbergValue {
  double nonNormalizedAuth;
  double nonNormalizedHub;
  double normalizedAuth;
  double normalizedHub;
};

template<typename Inspector>
auto inspect(Inspector& f, HITSKleinbergValue& v) {
  return f.object(v).fields(f.field("nonNormalizedAuth", v.nonNormalizedAuth),
                            f.field("nonNormalizedHub", v.nonNormalizedHub),
                            f.field("normalizedAuth", v.normalizedAuth),
                            f.field("normalizedHub", v.normalizedHub));
}

}  // namespace arangodb::pregel::algos
