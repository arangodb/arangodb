////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_RANDOM_UNIFORM_CHARACTER_H
#define ARANGODB_RANDOM_UNIFORM_CHARACTER_H 1

#include "Basics/Common.h"

namespace arangodb {
class UniformCharacter {
 private:
  UniformCharacter(UniformCharacter const&);
  UniformCharacter& operator=(UniformCharacter const&);

 public:
  explicit UniformCharacter(size_t length);
  explicit UniformCharacter(std::string const& characters);
  UniformCharacter(size_t length, std::string const& characters);

 public:
  std::string random();
  std::string random(size_t length);
  char randomChar() const;

 private:
  size_t _length;
  std::string const _characters;
};
}  // namespace arangodb

#endif
