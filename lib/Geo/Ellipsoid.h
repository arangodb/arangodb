////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GEO_ELLIPSOID_H
#define ARANGOD_GEO_ELLIPSOID_H 1

#include <cstddef>

namespace arangodb {
namespace geo {

class Ellipsoid {
public:
    Ellipsoid(double radius, double flattening)
    : _equatorRadius(radius), _flattening(flattening) { }
    
    // In meters
    inline double equator_radius() const noexcept {
        return _equatorRadius;
    }
    // In meters
    inline double poles_radius() const noexcept {
        return (1.0 - _flattening) * _equatorRadius;
    }
    // Flattening, see http://en.wikipedia.org/w/index.php?title=Flattening&oldid=602517763
    inline double flattening() const noexcept {
        return _flattening;
    }
    
private:
    double _equatorRadius;
    double _flattening;
};

// WGS 84 is a commonly used standard for earth geometry, see
// http://en.wikipedia.org/w/index.php?title=World_Geodetic_System&oldid=614370148
static const Ellipsoid WGS84_ELLIPSOID(6378137.0, 1.0/298.257223563);
static const Ellipsoid SPHERE((6371.000 * 1000), 0.0);

namespace utils {
Ellipsoid const& ellipsoidFromString(const char* ptr, std::size_t len);
}

}  // namespace geo
}  // namespace arangodb

#endif  // ARANGOD_GEO_ELLIPSOID_H

