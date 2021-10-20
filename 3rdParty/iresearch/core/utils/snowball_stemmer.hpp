////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////

#ifndef SNOWBALL_STEMMER_H
#define SNOWBALL_STEMMER_H

#include <memory>

struct sb_stemmer;

namespace iresearch {

struct stemmer_deleter {
   void operator()(sb_stemmer* p) const noexcept;
};

using stemmer_ptr = std::unique_ptr<sb_stemmer, stemmer_deleter>;

stemmer_ptr make_stemmer_ptr(const char * algorithm, const char * charenc);

} // iresearch

#endif // SNOWBALL_STEMMER_H
