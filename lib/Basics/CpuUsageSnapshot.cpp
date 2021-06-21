////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "CpuUsageSnapshot.h"
#include "Basics/NumberUtils.h"

#include <algorithm>

namespace arangodb {

CpuUsageSnapshot CpuUsageSnapshot::fromString(char const* buffer, std::size_t bufferSize) noexcept {
  auto readNumber = [](char const*& p, char const* e, bool& valid) {
    if (p >= e) {
      valid = false;
    }
    
    if (!valid) {
      // nothing to do
      return uint64_t(0);
    }

    // skip over initial whitespace
    while (p < e && *p == ' ') {
      ++p;
    }

    // remember start position of number
    char const* s = p;
    // skip over number to find its end
    while (p < e && *p >= '0' && *p <= '9') { 
      ++p;
    }
    return arangodb::NumberUtils::atoi_positive<uint64_t>(s, p, valid);
  };
  
  char const* s = buffer;
  char const* e = s + bufferSize;

  bool valid = true;
  
  CpuUsageSnapshot snap;
  
  snap.user = readNumber(s, e, valid);
  snap.nice = readNumber(s, e, valid);
  snap.system = readNumber(s, e, valid);
  snap.idle = readNumber(s, e, valid);
  snap.iowait = readNumber(s, e, valid);
  snap.irq = readNumber(s, e, valid);
  snap.softirq = readNumber(s, e, valid);
  snap.steal = readNumber(s, e, valid);
  snap.guest = readNumber(s, e, valid);
  snap.guestnice = readNumber(s, e, valid);

  if (!valid) {
    snap.clear();
  }

  return snap;
}

void CpuUsageSnapshot::subtract(CpuUsageSnapshot const& other) noexcept {
  // prevent underflow of values
  user -= std::min(user, other.user);
  nice -= std::min(nice, other.nice);
  system -= std::min(system, other.system);
  idle -= std::min(idle, other.idle);
  iowait -= std::min(iowait, other.iowait);
  irq -= std::min(irq, other.irq);
  softirq -= std::min(softirq, other.softirq);
  steal -= std::min(steal, other.steal);
  guest -= std::min(guest, other.guest);
  guestnice -= std::min(guestnice, other.guestnice);
}

bool CpuUsageSnapshot::valid() const noexcept {
  return total() > 0;
}

void CpuUsageSnapshot::clear() noexcept {
  user = 0;
  nice = 0;
  system = 0;
  idle = 0;
  iowait = 0;
  irq = 0;
  softirq = 0;
  steal = 0;
  guest = 0;
  guestnice = 0;
}

uint64_t CpuUsageSnapshot::total() const noexcept {
  return user + nice + system + idle + iowait + irq + softirq + steal + guest + guestnice;
}

}  // namespace arangodb
