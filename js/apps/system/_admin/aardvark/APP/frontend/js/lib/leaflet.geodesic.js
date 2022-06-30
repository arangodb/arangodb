/*! Leaflet.Geodesic 2.6.1 - (c) Henry Thasler - https://github.com/henrythasler/Leaflet.Geodesic */
'use strict';

Object.defineProperty(exports, '__esModule', { value: true });

function _interopNamespace(e) {
    if (e && e.__esModule) return e;
    var n = Object.create(null);
    if (e) {
        Object.keys(e).forEach(function (k) {
            if (k !== 'default') {
                var d = Object.getOwnPropertyDescriptor(e, k);
                Object.defineProperty(n, k, d.get ? d : {
                    enumerable: true,
                    get: function () { return e[k]; }
                });
            }
        });
    }
    n["default"] = e;
    return Object.freeze(n);
}

var L__namespace = /*#__PURE__*/_interopNamespace(L);

/*! *****************************************************************************
Copyright (c) Microsoft Corporation.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
***************************************************************************** */
/* global Reflect, Promise */

var extendStatics = function(d, b) {
    extendStatics = Object.setPrototypeOf ||
        ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
        function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
    return extendStatics(d, b);
};

function __extends(d, b) {
    if (typeof b !== "function" && b !== null)
        throw new TypeError("Class extends value " + String(b) + " is not a constructor or null");
    extendStatics(d, b);
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
}

var __assign = function() {
    __assign = Object.assign || function __assign(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
            s = arguments[i];
            for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p)) t[p] = s[p];
        }
        return t;
    };
    return __assign.apply(this, arguments);
};

function __spreadArray(to, from, pack) {
    if (pack || arguments.length === 2) for (var i = 0, l = from.length, ar; i < l; i++) {
        if (ar || !(i in from)) {
            if (!ar) ar = Array.prototype.slice.call(from, 0, i);
            ar[i] = from[i];
        }
    }
    return to.concat(ar || Array.prototype.slice.call(from));
}

var GeodesicCore = /** @class */ (function () {
    function GeodesicCore(options) {
        this.options = { wrap: true, steps: 3 };
        this.ellipsoid = {
            a: 6378137,
            b: 6356752.3142,
            f: 1 / 298.257223563
        }; // WGS-84
        this.options = __assign(__assign({}, this.options), options);
    }
    GeodesicCore.prototype.toRadians = function (degree) {
        return degree * Math.PI / 180;
    };
    GeodesicCore.prototype.toDegrees = function (radians) {
        return radians * 180 / Math.PI;
    };
    /**
     * implements scientific modulus
     * source: http://www.codeavenger.com/2017/05/19/JavaScript-Modulo-operation-and-the-Caesar-Cipher.html
     * @param n
     * @param p
     * @return
     */
    GeodesicCore.prototype.mod = function (n, p) {
        var r = n % p;
        return r < 0 ? r + p : r;
    };
    /**
     * source: https://github.com/chrisveness/geodesy/blob/master/dms.js
     * @param degrees arbitrary value
     * @return degrees between 0..360
     */
    GeodesicCore.prototype.wrap360 = function (degrees) {
        if (0 <= degrees && degrees < 360) {
            return degrees; // avoid rounding due to arithmetic ops if within range
        }
        else {
            return this.mod(degrees, 360);
        }
    };
    /**
     * general wrap function with arbitrary max value
     * @param degrees arbitrary value
     * @param max
     * @return degrees between `-max`..`+max`
     */
    GeodesicCore.prototype.wrap = function (degrees, max) {
        if (max === void 0) { max = 360; }
        if (-max <= degrees && degrees <= max) {
            return degrees;
        }
        else {
            return this.mod((degrees + max), 2 * max) - max;
        }
    };
    /**
     * Vincenty direct calculation.
     * based on the work of Chris Veness (https://github.com/chrisveness/geodesy)
     * source: https://github.com/chrisveness/geodesy/blob/master/latlon-ellipsoidal-vincenty.js
     *
     * @param start starting point
     * @param bearing initial bearing (in degrees)
     * @param distance distance from starting point to calculate along given bearing in meters.
     * @param maxInterations How many iterations can be made to reach the allowed deviation (`ε`), before an error will be thrown.
     * @return Final point (destination point) and bearing (in degrees)
     */
    GeodesicCore.prototype.direct = function (start, bearing, distance, maxInterations) {
        if (maxInterations === void 0) { maxInterations = 100; }
        var φ1 = this.toRadians(start.lat);
        var λ1 = this.toRadians(start.lng);
        var α1 = this.toRadians(bearing);
        var s = distance;
        var ε = Number.EPSILON * 1000;
        var _a = this.ellipsoid, a = _a.a, b = _a.b, f = _a.f;
        var sinα1 = Math.sin(α1);
        var cosα1 = Math.cos(α1);
        var tanU1 = (1 - f) * Math.tan(φ1), cosU1 = 1 / Math.sqrt((1 + tanU1 * tanU1)), sinU1 = tanU1 * cosU1;
        var σ1 = Math.atan2(tanU1, cosα1); // σ1 = angular distance on the sphere from the equator to P1
        var sinα = cosU1 * sinα1; // α = azimuth of the geodesic at the equator
        var cosSqα = 1 - sinα * sinα;
        var uSq = cosSqα * (a * a - b * b) / (b * b);
        var A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
        var B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
        var σ = s / (b * A), sinσ = null, cosσ = null, Δσ = null; // σ = angular distance P₁ P₂ on the sphere
        var cos2σₘ = null; // σₘ = angular distance on the sphere from the equator to the midpoint of the line
        var σʹ = null, iterations = 0;
        do {
            cos2σₘ = Math.cos(2 * σ1 + σ);
            sinσ = Math.sin(σ);
            cosσ = Math.cos(σ);
            Δσ = B * sinσ * (cos2σₘ + B / 4 * (cosσ * (-1 + 2 * cos2σₘ * cos2σₘ) -
                B / 6 * cos2σₘ * (-3 + 4 * sinσ * sinσ) * (-3 + 4 * cos2σₘ * cos2σₘ)));
            σʹ = σ;
            σ = s / (b * A) + Δσ;
        } while (Math.abs(σ - σʹ) > ε && ++iterations < maxInterations);
        if (iterations >= maxInterations) {
            throw new EvalError("Direct vincenty formula failed to converge after ".concat(maxInterations, " iterations \n                (start=").concat(start.lat, "/").concat(start.lng, "; bearing=").concat(bearing, "; distance=").concat(distance, ")")); // not possible?
        }
        var x = sinU1 * sinσ - cosU1 * cosσ * cosα1;
        var φ2 = Math.atan2(sinU1 * cosσ + cosU1 * sinσ * cosα1, (1 - f) * Math.sqrt(sinα * sinα + x * x));
        var λ = Math.atan2(sinσ * sinα1, cosU1 * cosσ - sinU1 * sinσ * cosα1);
        var C = f / 16 * cosSqα * (4 + f * (4 - 3 * cosSqα));
        var dL = λ - (1 - C) * f * sinα * (σ + C * sinσ * (cos2σₘ + C * cosσ * (-1 + 2 * cos2σₘ * cos2σₘ)));
        var λ2 = λ1 + dL;
        var α2 = Math.atan2(sinα, -x);
        return {
            lat: this.toDegrees(φ2),
            lng: this.toDegrees(λ2),
            bearing: this.wrap360(this.toDegrees(α2))
        };
    };
    /**
     * Vincenty inverse calculation.
     * based on the work of Chris Veness (https://github.com/chrisveness/geodesy)
     * source: https://github.com/chrisveness/geodesy/blob/master/latlon-ellipsoidal-vincenty.js
     *
     * @param start Latitude/longitude of starting point.
     * @param dest Latitude/longitude of destination point.
     * @return Object including distance, initialBearing, finalBearing.
     */
    GeodesicCore.prototype.inverse = function (start, dest, maxInterations, mitigateConvergenceError) {
        if (maxInterations === void 0) { maxInterations = 100; }
        if (mitigateConvergenceError === void 0) { mitigateConvergenceError = true; }
        var p1 = start, p2 = dest;
        var φ1 = this.toRadians(p1.lat), λ1 = this.toRadians(p1.lng);
        var φ2 = this.toRadians(p2.lat), λ2 = this.toRadians(p2.lng);
        var π = Math.PI;
        var ε = Number.EPSILON;
        // allow alternative ellipsoid to be specified
        var _a = this.ellipsoid, a = _a.a, b = _a.b, f = _a.f;
        var dL = λ2 - λ1; // L = difference in longitude, U = reduced latitude, defined by tan U = (1-f)·tanφ.
        var tanU1 = (1 - f) * Math.tan(φ1), cosU1 = 1 / Math.sqrt((1 + tanU1 * tanU1)), sinU1 = tanU1 * cosU1;
        var tanU2 = (1 - f) * Math.tan(φ2), cosU2 = 1 / Math.sqrt((1 + tanU2 * tanU2)), sinU2 = tanU2 * cosU2;
        var antipodal = Math.abs(dL) > π / 2 || Math.abs(φ2 - φ1) > π / 2;
        var λ = dL, sinλ = null, cosλ = null; // λ = difference in longitude on an auxiliary sphere
        var σ = antipodal ? π : 0, sinσ = 0, cosσ = antipodal ? -1 : 1, sinSqσ = null; // σ = angular distance P₁ P₂ on the sphere
        var cos2σₘ = 1; // σₘ = angular distance on the sphere from the equator to the midpoint of the line
        var sinα = null, cosSqα = 1; // α = azimuth of the geodesic at the equator
        var C = null;
        var λʹ = null, iterations = 0;
        do {
            sinλ = Math.sin(λ);
            cosλ = Math.cos(λ);
            sinSqσ = (cosU2 * sinλ) * (cosU2 * sinλ) + (cosU1 * sinU2 - sinU1 * cosU2 * cosλ) * (cosU1 * sinU2 - sinU1 * cosU2 * cosλ);
            if (Math.abs(sinSqσ) < ε) {
                break; // co-incident/antipodal points (falls back on λ/σ = L)
            }
            sinσ = Math.sqrt(sinSqσ);
            cosσ = sinU1 * sinU2 + cosU1 * cosU2 * cosλ;
            σ = Math.atan2(sinσ, cosσ);
            sinα = cosU1 * cosU2 * sinλ / sinσ;
            cosSqα = 1 - sinα * sinα;
            cos2σₘ = (cosSqα !== 0) ? (cosσ - 2 * sinU1 * sinU2 / cosSqα) : 0; // on equatorial line cos²α = 0 (§6)
            C = f / 16 * cosSqα * (4 + f * (4 - 3 * cosSqα));
            λʹ = λ;
            λ = dL + (1 - C) * f * sinα * (σ + C * sinσ * (cos2σₘ + C * cosσ * (-1 + 2 * cos2σₘ * cos2σₘ)));
            var iterationCheck = antipodal ? Math.abs(λ) - π : Math.abs(λ);
            if (iterationCheck > π) {
                throw new EvalError('λ > π');
            }
        } while (Math.abs(λ - λʹ) > 1e-12 && ++iterations < maxInterations);
        if (iterations >= maxInterations) {
            if (mitigateConvergenceError) {
                return this.inverse(start, new L__namespace.LatLng(dest.lat, dest.lng - 0.01), maxInterations, mitigateConvergenceError);
            }
            else {
                throw new EvalError("Inverse vincenty formula failed to converge after ".concat(maxInterations, " iterations \n                    (start=").concat(start.lat, "/").concat(start.lng, "; dest=").concat(dest.lat, "/").concat(dest.lng, ")"));
            }
        }
        var uSq = cosSqα * (a * a - b * b) / (b * b);
        var A = 1 + uSq / 16384 * (4096 + uSq * (-768 + uSq * (320 - 175 * uSq)));
        var B = uSq / 1024 * (256 + uSq * (-128 + uSq * (74 - 47 * uSq)));
        var Δσ = B * sinσ * (cos2σₘ + B / 4 * (cosσ * (-1 + 2 * cos2σₘ * cos2σₘ) -
            B / 6 * cos2σₘ * (-3 + 4 * sinσ * sinσ) * (-3 + 4 * cos2σₘ * cos2σₘ)));
        var s = b * A * (σ - Δσ); // s = length of the geodesic
        // note special handling of exactly antipodal points where sin²σ = 0 (due to discontinuity
        // atan2(0, 0) = 0 but atan2(this.ε, 0) = π/2 / 90°) - in which case bearing is always meridional,
        // due north (or due south!)
        // α = azimuths of the geodesic; α2 the direction P₁ P₂ produced
        var α1 = Math.abs(sinSqσ) < ε ? 0 : Math.atan2(cosU2 * sinλ, cosU1 * sinU2 - sinU1 * cosU2 * cosλ);
        var α2 = Math.abs(sinSqσ) < ε ? π : Math.atan2(cosU1 * sinλ, -sinU1 * cosU2 + cosU1 * sinU2 * cosλ);
        return {
            distance: s,
            initialBearing: Math.abs(s) < ε ? NaN : this.wrap360(this.toDegrees(α1)),
            finalBearing: Math.abs(s) < ε ? NaN : this.wrap360(this.toDegrees(α2))
        };
    };
    /**
     * Returns the point of intersection of two paths defined by position and bearing.
     * This calculation uses a spherical model of the earth. This will lead to small errors compared to an ellipsiod model.
     * based on the work of Chris Veness (https://github.com/chrisveness/geodesy)
     * source: https://github.com/chrisveness/geodesy/blob/master/latlon-spherical.js
     *
     * @param firstPos 1st path: position and bearing
     * @param firstBearing
     * @param secondPos 2nd path: position and bearing
     * @param secondBearing
     */
    GeodesicCore.prototype.intersection = function (firstPos, firstBearing, secondPos, secondBearing) {
        var φ1 = this.toRadians(firstPos.lat);
        var λ1 = this.toRadians(firstPos.lng);
        var φ2 = this.toRadians(secondPos.lat);
        var λ2 = this.toRadians(secondPos.lng);
        var θ13 = this.toRadians(firstBearing);
        var θ23 = this.toRadians(secondBearing);
        var Δφ = φ2 - φ1, Δλ = λ2 - λ1;
        var π = Math.PI;
        var ε = Number.EPSILON;
        // angular distance p1-p2
        var δ12 = 2 * Math.asin(Math.sqrt(Math.sin(Δφ / 2) * Math.sin(Δφ / 2)
            + Math.cos(φ1) * Math.cos(φ2) * Math.sin(Δλ / 2) * Math.sin(Δλ / 2)));
        if (Math.abs(δ12) < ε) {
            return firstPos; // coincident points
        }
        // initial/final bearings between points
        var cosθa = (Math.sin(φ2) - Math.sin(φ1) * Math.cos(δ12)) / (Math.sin(δ12) * Math.cos(φ1));
        var cosθb = (Math.sin(φ1) - Math.sin(φ2) * Math.cos(δ12)) / (Math.sin(δ12) * Math.cos(φ2));
        var θa = Math.acos(Math.min(Math.max(cosθa, -1), 1)); // protect against rounding errors
        var θb = Math.acos(Math.min(Math.max(cosθb, -1), 1)); // protect against rounding errors
        var θ12 = Math.sin(λ2 - λ1) > 0 ? θa : 2 * π - θa;
        var θ21 = Math.sin(λ2 - λ1) > 0 ? 2 * π - θb : θb;
        var α1 = θ13 - θ12; // angle 2-1-3
        var α2 = θ21 - θ23; // angle 1-2-3
        if (Math.sin(α1) === 0 && Math.sin(α2) === 0) {
            return null; // infinite intersections
        }
        if (Math.sin(α1) * Math.sin(α2) < 0) {
            return null; // ambiguous intersection (antipodal?)
        }
        var cosα3 = -Math.cos(α1) * Math.cos(α2) + Math.sin(α1) * Math.sin(α2) * Math.cos(δ12);
        var δ13 = Math.atan2(Math.sin(δ12) * Math.sin(α1) * Math.sin(α2), Math.cos(α2) + Math.cos(α1) * cosα3);
        var φ3 = Math.asin(Math.min(Math.max(Math.sin(φ1) * Math.cos(δ13) + Math.cos(φ1) * Math.sin(δ13) * Math.cos(θ13), -1), 1));
        var Δλ13 = Math.atan2(Math.sin(θ13) * Math.sin(δ13) * Math.cos(φ1), Math.cos(δ13) - Math.sin(φ1) * Math.sin(φ3));
        var λ3 = λ1 + Δλ13;
        return new L__namespace.LatLng(this.toDegrees(φ3), this.toDegrees(λ3));
    };
    GeodesicCore.prototype.midpoint = function (start, dest) {
        // φm = atan2( sinφ1 + sinφ2, √( (cosφ1 + cosφ2⋅cosΔλ)² + cos²φ2⋅sin²Δλ ) )
        // λm = λ1 + atan2(cosφ2⋅sinΔλ, cosφ1 + cosφ2⋅cosΔλ)
        // midpoint is sum of vectors to two points: mathforum.org/library/drmath/view/51822.html
        var φ1 = this.toRadians(start.lat);
        var λ1 = this.toRadians(start.lng);
        var φ2 = this.toRadians(dest.lat);
        var Δλ = this.toRadians(dest.lng - start.lng);
        // get cartesian coordinates for the two points
        var A = { x: Math.cos(φ1), y: 0, z: Math.sin(φ1) }; // place point A on prime meridian y=0
        var B = { x: Math.cos(φ2) * Math.cos(Δλ), y: Math.cos(φ2) * Math.sin(Δλ), z: Math.sin(φ2) };
        // vector to midpoint is sum of vectors to two points (no need to normalise)
        var C = { x: A.x + B.x, y: A.y + B.y, z: A.z + B.z };
        var φm = Math.atan2(C.z, Math.sqrt(C.x * C.x + C.y * C.y));
        var λm = λ1 + Math.atan2(C.y, C.x);
        return new L__namespace.LatLng(this.toDegrees(φm), this.toDegrees(λm));
    };
    return GeodesicCore;
}());

var GeodesicGeometry = /** @class */ (function () {
    function GeodesicGeometry(options) {
        this.geodesic = new GeodesicCore();
        this.steps = (options && options.steps !== undefined) ? options.steps : 3;
    }
    /**
     * A geodesic line between `start` and `dest` is created with this recursive function.
     * It calculates the geodesic midpoint between `start` and `dest` and uses this midpoint to call itself again (twice!).
     * The results are then merged into one continuous linestring.
     *
     * The number of resulting vertices (incl. `start` and `dest`) depends on the initial value for `iterations`
     * and can be calculated with: vertices == 1 + 2 ** (initialIterations + 1)
     *
     * As this is an exponential function, be extra careful to limit the initial value for `iterations` (8 results in 513 vertices).
     *
     * @param start start position
     * @param dest destination
     * @param iterations
     * @return resulting linestring
     */
    GeodesicGeometry.prototype.recursiveMidpoint = function (start, dest, iterations) {
        var geom = [start, dest];
        var midpoint = this.geodesic.midpoint(start, dest);
        if (iterations > 0) {
            geom.splice.apply(geom, __spreadArray([0, 1], this.recursiveMidpoint(start, midpoint, iterations - 1), false));
            geom.splice.apply(geom, __spreadArray([geom.length - 2, 2], this.recursiveMidpoint(midpoint, dest, iterations - 1), false));
        }
        else {
            geom.splice(1, 0, midpoint);
        }
        return geom;
    };
    /**
     * This is the wrapper-function to generate a geodesic line. It's just for future backwards-compatibility
     * if there is another algorithm used to create the actual line.
     *
     * The `steps`-property is used to define the number of resulting vertices of the linestring: vertices == 1 + 2 ** (steps + 1)
     * The value for `steps` is currently limited to 8 (513 vertices) for performance reasons until another algorithm is found.
     *
     * @param start start position
     * @param dest destination
     * @return resulting linestring
     */
    GeodesicGeometry.prototype.line = function (start, dest) {
        return this.recursiveMidpoint(start, dest, Math.min(8, this.steps));
    };
    GeodesicGeometry.prototype.multiLineString = function (latlngs) {
        var multiLineString = [];
        for (var _i = 0, latlngs_1 = latlngs; _i < latlngs_1.length; _i++) {
            var linestring = latlngs_1[_i];
            var segment = [];
            for (var j = 1; j < linestring.length; j++) {
                segment.splice.apply(segment, __spreadArray([segment.length - 1, 1], this.line(linestring[j - 1], linestring[j]), false));
            }
            multiLineString.push(segment);
        }
        return multiLineString;
    };
    GeodesicGeometry.prototype.lineString = function (latlngs) {
        return this.multiLineString([latlngs])[0];
    };
    /**
     *
     * Is much (10x) faster than the previous implementation:
     *
     * ```
     * Benchmark (no split):  splitLine x 459,044 ops/sec ±0.53% (95 runs sampled)
     * Benchmark (split):     splitLine x 42,999 ops/sec ±0.51% (97 runs sampled)
     * ```
     *
     * @param startPosition
     * @param destPosition
     */
    GeodesicGeometry.prototype.splitLine = function (startPosition, destPosition) {
        var antimeridianWest = {
            point: new L__namespace.LatLng(89.9, -180.0000001),
            bearing: 180
        };
        var antimeridianEast = {
            point: new L__namespace.LatLng(89.9, 180.0000001),
            bearing: 180
        };
        // make a copy to work with
        var start = new L__namespace.LatLng(startPosition.lat, startPosition.lng);
        var dest = new L__namespace.LatLng(destPosition.lat, destPosition.lng);
        start.lng = this.geodesic.wrap(start.lng, 360);
        dest.lng = this.geodesic.wrap(dest.lng, 360);
        if ((dest.lng - start.lng) > 180) {
            dest.lng = dest.lng - 360;
        }
        else if ((dest.lng - start.lng) < -180) {
            dest.lng = dest.lng + 360;
        }
        var result = [[new L__namespace.LatLng(start.lat, this.geodesic.wrap(start.lng, 180)), new L__namespace.LatLng(dest.lat, this.geodesic.wrap(dest.lng, 180))]];
        // crossing antimeridian from "this" side?
        if ((start.lng >= -180) && (start.lng <= 180)) {
            // crossing the "western" antimeridian
            if (dest.lng < -180) {
                var bearing = this.geodesic.inverse(start, dest).initialBearing;
                var intersection = this.geodesic.intersection(start, bearing, antimeridianWest.point, antimeridianWest.bearing);
                if (intersection) {
                    result = [[start, intersection], [new L__namespace.LatLng(intersection.lat, intersection.lng + 360), new L__namespace.LatLng(dest.lat, dest.lng + 360)]];
                }
            }
            // crossing the "eastern" antimeridian
            else if (dest.lng > 180) {
                var bearing = this.geodesic.inverse(start, dest).initialBearing;
                var intersection = this.geodesic.intersection(start, bearing, antimeridianEast.point, antimeridianEast.bearing);
                if (intersection) {
                    result = [[start, intersection], [new L__namespace.LatLng(intersection.lat, intersection.lng - 360), new L__namespace.LatLng(dest.lat, dest.lng - 360)]];
                }
            }
        }
        // coming back over the antimeridian from the "other" side?
        else if ((dest.lng >= -180) && (dest.lng <= 180)) {
            // crossing the "western" antimeridian
            if (start.lng < -180) {
                var bearing = this.geodesic.inverse(start, dest).initialBearing;
                var intersection = this.geodesic.intersection(start, bearing, antimeridianWest.point, antimeridianWest.bearing);
                if (intersection) {
                    result = [[new L__namespace.LatLng(start.lat, start.lng + 360), new L__namespace.LatLng(intersection.lat, intersection.lng + 360)], [intersection, dest]];
                }
            }
            // crossing the "eastern" antimeridian
            else if (start.lng > 180) {
                var bearing = this.geodesic.inverse(start, dest).initialBearing;
                var intersection = this.geodesic.intersection(start, bearing, antimeridianWest.point, antimeridianWest.bearing);
                if (intersection) {
                    result = [[new L__namespace.LatLng(start.lat, start.lng - 360), new L__namespace.LatLng(intersection.lat, intersection.lng - 360)], [intersection, dest]];
                }
            }
        }
        return result;
    };
    /**
     * Linestrings of a given multilinestring that cross the antimeridian will be split in two separate linestrings.
     * This function is used to wrap lines around when they cross the antimeridian
     * It iterates over all linestrings and reconstructs the step-by-step if no split is needed.
     * In case the line was split, the linestring ends at the antimeridian and a new linestring is created for the
     * remaining points of the original linestring.
     *
     * @param multilinestring
     * @return another multilinestring where segments crossing the antimeridian are split
     */
    GeodesicGeometry.prototype.splitMultiLineString = function (multilinestring) {
        var result = [];
        for (var _i = 0, multilinestring_1 = multilinestring; _i < multilinestring_1.length; _i++) {
            var linestring = multilinestring_1[_i];
            if (linestring.length === 1) {
                result.push(linestring); // just a single point in linestring, no need to split
                continue;
            }
            var segment = [];
            for (var j = 1; j < linestring.length; j++) {
                var split = this.splitLine(linestring[j - 1], linestring[j]);
                segment.pop();
                segment = segment.concat(split[0]);
                if (split.length > 1) {
                    result.push(segment); // the line was split, so we end the linestring right here
                    segment = split[1]; // begin the new linestring with the second part of the split line
                }
            }
            result.push(segment);
        }
        return result;
    };
    /**
     * Linestrings of a given multilinestring will be wrapped (+- 360°) to show a continuous line w/o any weird discontinuities
     * when `wrap` is set to `false` in the geodesic class
     * @param multilinestring
     * @returns another multilinestring where the points of each linestring are wrapped accordingly
     */
    GeodesicGeometry.prototype.wrapMultiLineString = function (multilinestring) {
        var result = [];
        for (var _i = 0, multilinestring_2 = multilinestring; _i < multilinestring_2.length; _i++) {
            var linestring = multilinestring_2[_i];
            var resultLine = [];
            var previous = null;
            // iterate over every point and check if it needs to be wrapped
            for (var _a = 0, linestring_1 = linestring; _a < linestring_1.length; _a++) {
                var point = linestring_1[_a];
                if (previous === null) {
                    // the first point is the anchor of the linestring from which the line will always start (w/o any wrapping applied)
                    resultLine.push(new L__namespace.LatLng(point.lat, point.lng));
                    previous = new L__namespace.LatLng(point.lat, point.lng);
                }
                else { // I prefer clearly defined branches over a continue-operation.
                    // if the difference between the current and *previous* point is greater than 360°, the current point needs to be shifted
                    // to be on the same 'sphere' as the previous one.
                    var offset = Math.round((point.lng - previous.lng) / 360);
                    // shift the point accordingly and add to the result
                    resultLine.push(new L__namespace.LatLng(point.lat, point.lng - offset * 360));
                    // use the wrapped point as the anchor for the next one
                    previous = new L__namespace.LatLng(point.lat, point.lng - offset * 360); // Need a new object here, to avoid changing the input data
                }
            }
            result.push(resultLine);
        }
        return result;
    };
    /**
     * Creates a circular (constant radius), closed (1st pos == last pos) geodesic linestring.
     * The number of vertices is calculated with: `vertices == steps + 1` (where 1st == last)
     *
     * @param center
     * @param radius
     * @return resulting linestring
     */
    GeodesicGeometry.prototype.circle = function (center, radius) {
        var vertices = [];
        for (var i = 0; i < this.steps; i++) {
            var point = this.geodesic.direct(center, 360 / this.steps * i, radius);
            vertices.push(new L__namespace.LatLng(point.lat, point.lng));
        }
        // append first vertice to the end to close the linestring
        vertices.push(new L__namespace.LatLng(vertices[0].lat, vertices[0].lng));
        return vertices;
    };
    /**
     * Handles splitting of circles at the antimeridian.
     * @param linestring a linestring that resembles the geodesic circle
     * @return a multilinestring that consist of one or two linestrings
     */
    GeodesicGeometry.prototype.splitCircle = function (linestring) {
        var result = this.splitMultiLineString([linestring]);
        // If the circle was split, it results in exactly three linestrings where first and last  
        // must be re-assembled because they belong to the same "side" of the split circle.
        if (result.length === 3) {
            result[2] = __spreadArray(__spreadArray([], result[2], true), result[0], true);
            result.shift();
        }
        return result;
    };
    /**
     * Calculates the distance between two positions on the earths surface
     * @param start 1st position
     * @param dest 2nd position
     * @return the distance in **meters**
     */
    GeodesicGeometry.prototype.distance = function (start, dest) {
        return this.geodesic.inverse(new L__namespace.LatLng(start.lat, this.geodesic.wrap(start.lng, 180)), new L__namespace.LatLng(dest.lat, this.geodesic.wrap(dest.lng, 180))).distance;
    };
    GeodesicGeometry.prototype.multilineDistance = function (multilinestring) {
        var dist = [];
        for (var _i = 0, multilinestring_3 = multilinestring; _i < multilinestring_3.length; _i++) {
            var linestring = multilinestring_3[_i];
            var segmentDistance = 0;
            for (var j = 1; j < linestring.length; j++) {
                segmentDistance += this.distance(linestring[j - 1], linestring[j]);
            }
            dist.push(segmentDistance);
        }
        return dist;
    };
    GeodesicGeometry.prototype.updateStatistics = function (points, vertices) {
        var stats = {};
        stats.distanceArray = this.multilineDistance(points);
        stats.totalDistance = stats.distanceArray.reduce(function (x, y) { return x + y; }, 0);
        stats.points = 0;
        for (var _i = 0, points_1 = points; _i < points_1.length; _i++) {
            var item = points_1[_i];
            stats.points += item.reduce(function (x) { return x + 1; }, 0);
        }
        stats.vertices = 0;
        for (var _a = 0, vertices_1 = vertices; _a < vertices_1.length; _a++) {
            var item = vertices_1[_a];
            stats.vertices += item.reduce(function (x) { return x + 1; }, 0);
        }
        return stats;
    };
    return GeodesicGeometry;
}());

function instanceOfLatLngLiteral(object) {
    return ((typeof object === "object")
        && (object !== null)
        && ('lat' in object)
        && ('lng' in object)
        && (typeof object.lat === "number")
        && (typeof object.lng === "number"));
}
function instanceOfLatLngTuple(object) {
    return ((object instanceof Array)
        && (typeof object[0] === "number")
        && (typeof object[1] === "number"));
}
function instanceOfLatLngExpression(object) {
    return object instanceof L__namespace.LatLng || instanceOfLatLngTuple(object) || instanceOfLatLngLiteral(object);
}
function latlngExpressiontoLatLng(input) {
    if (input instanceof L__namespace.LatLng) {
        return input;
    }
    else if (instanceOfLatLngTuple(input)) {
        return new L__namespace.LatLng(input[0], input[1]);
    }
    else if (instanceOfLatLngLiteral(input)) {
        return new L__namespace.LatLng(input.lat, input.lng);
    }
    throw new Error("L.LatLngExpression expected. Unknown object found.");
}
function latlngExpressionArraytoLatLngArray(input) {
    var latlng = [];
    var iterateOver = (instanceOfLatLngExpression(input[0]) ? [input] : input);
    var unknownObjectError = new Error("L.LatLngExpression[] | L.LatLngExpression[][] expected. Unknown object found.");
    if (!(iterateOver instanceof Array)) {
        throw unknownObjectError;
    }
    for (var _i = 0, _a = iterateOver; _i < _a.length; _i++) {
        var group = _a[_i];
        if (!(group instanceof Array)) {
            throw unknownObjectError;
        }
        var sub = [];
        for (var _b = 0, group_1 = group; _b < group_1.length; _b++) {
            var point = group_1[_b];
            if (!instanceOfLatLngExpression(point)) {
                throw unknownObjectError;
            }
            sub.push(latlngExpressiontoLatLng(point));
        }
        latlng.push(sub);
    }
    return latlng;
}

/**
 * Draw geodesic lines based on L.Polyline
 */
var GeodesicLine = /** @class */ (function (_super) {
    __extends(GeodesicLine, _super);
    function GeodesicLine(latlngs, options) {
        var _this = _super.call(this, [], options) || this;
        /** these should be good for most use-cases */
        _this.defaultOptions = { wrap: true, steps: 3 };
        /** use this if you need some detailled info about the current geometry */
        _this.statistics = {};
        /** stores all positions that are used to create the geodesic line */
        _this.points = [];
        L__namespace.Util.setOptions(_this, __assign(__assign({}, _this.defaultOptions), options));
        _this.geom = new GeodesicGeometry(_this.options);
        if (latlngs !== undefined) {
            _this.setLatLngs(latlngs);
        }
        return _this;
    }
    /** calculates the geodesics and update the polyline-object accordingly */
    GeodesicLine.prototype.updateGeometry = function () {
        var geodesic = [];
        geodesic = this.geom.multiLineString(this.points);
        this.statistics = this.geom.updateStatistics(this.points, geodesic);
        if (this.options.wrap) {
            var split = this.geom.splitMultiLineString(geodesic);
            _super.prototype.setLatLngs.call(this, split);
        }
        else {
            _super.prototype.setLatLngs.call(this, this.geom.wrapMultiLineString(geodesic));
        }
    };
    /**
     * overwrites the original function with additional functionality to create a geodesic line
     * @param latlngs an array (or 2d-array) of positions
     */
    GeodesicLine.prototype.setLatLngs = function (latlngs) {
        this.points = latlngExpressionArraytoLatLngArray(latlngs);
        this.updateGeometry();
        return this;
    };
    /**
     * add a given point to the geodesic line object
     * @param latlng point to add. The point will always be added to the last linestring of a multiline
     * @param latlngs define a linestring to add the new point to. Read from points-property before (e.g. `line.addLatLng(Beijing, line.points[0]);`)
     */
    GeodesicLine.prototype.addLatLng = function (latlng, latlngs) {
        var point = latlngExpressiontoLatLng(latlng);
        if (this.points.length === 0) {
            this.points.push([point]);
        }
        else {
            if (latlngs === undefined) {
                this.points[this.points.length - 1].push(point);
            }
            else {
                latlngs.push(point);
            }
        }
        this.updateGeometry();
        return this;
    };
    /**
     * Creates geodesic lines from a given GeoJSON-Object.
     * @param input GeoJSON-Object
     */
    GeodesicLine.prototype.fromGeoJson = function (input) {
        var latlngs = [];
        var features = [];
        if (input.type === "FeatureCollection") {
            features = input.features;
        }
        else if (input.type === "Feature") {
            features = [input];
        }
        else if (["MultiPoint", "LineString", "MultiLineString", "Polygon", "MultiPolygon"].includes(input.type)) {
            features = [{
                    type: "Feature",
                    geometry: input,
                    properties: {}
                }];
        }
        else {
            console.log("[Leaflet.Geodesic] fromGeoJson() - Type \"".concat(input.type, "\" not supported."));
        }
        features.forEach(function (feature) {
            switch (feature.geometry.type) {
                case "MultiPoint":
                case "LineString":
                    latlngs = __spreadArray(__spreadArray([], latlngs, true), [L__namespace.GeoJSON.coordsToLatLngs(feature.geometry.coordinates, 0)], false);
                    break;
                case "MultiLineString":
                case "Polygon":
                    latlngs = __spreadArray(__spreadArray([], latlngs, true), L__namespace.GeoJSON.coordsToLatLngs(feature.geometry.coordinates, 1), true);
                    break;
                case "MultiPolygon":
                    feature.geometry.coordinates.forEach(function (item) {
                        latlngs = __spreadArray(__spreadArray([], latlngs, true), L__namespace.GeoJSON.coordsToLatLngs(item, 1), true);
                    });
                    break;
                default:
                    console.log("[Leaflet.Geodesic] fromGeoJson() - Type \"".concat(feature.geometry.type, "\" not supported."));
            }
        });
        if (latlngs.length) {
            this.setLatLngs(latlngs);
        }
        return this;
    };
    /**
     * Calculates the distance between two geo-positions
     * @param start 1st position
     * @param dest 2nd position
     * @return the distance in meters
     */
    GeodesicLine.prototype.distance = function (start, dest) {
        return this.geom.distance(latlngExpressiontoLatLng(start), latlngExpressiontoLatLng(dest));
    };
    return GeodesicLine;
}(L__namespace.Polyline));

/**
 * Can be used to create a geodesic circle based on L.Polyline
 */
var GeodesicCircleClass = /** @class */ (function (_super) {
    __extends(GeodesicCircleClass, _super);
    function GeodesicCircleClass(center, options) {
        var _this = _super.call(this, [], options) || this;
        _this.defaultOptions = { wrap: true, steps: 24, fill: true, noClip: true };
        _this.statistics = {};
        L__namespace.Util.setOptions(_this, __assign(__assign({}, _this.defaultOptions), options));
        // merge/set options
        var extendedOptions = _this.options;
        _this.radius = (extendedOptions.radius === undefined) ? 1000 * 1000 : extendedOptions.radius;
        _this.center = (center === undefined) ? new L__namespace.LatLng(0, 0) : latlngExpressiontoLatLng(center);
        _this.geom = new GeodesicGeometry(_this.options);
        // update the geometry
        _this.update();
        return _this;
    }
    /**
     * Updates the geometry and re-calculates some statistics
     */
    GeodesicCircleClass.prototype.update = function () {
        var circle = this.geom.circle(this.center, this.radius);
        this.statistics = this.geom.updateStatistics([[this.center]], [circle]);
        // circumfence must be re-calculated from geodesic 
        this.statistics.totalDistance = this.geom.multilineDistance([circle]).reduce(function (x, y) { return x + y; }, 0);
        if (this.options.wrap) {
            var split = this.geom.splitCircle(circle);
            _super.prototype.setLatLngs.call(this, split);
        }
        else {
            _super.prototype.setLatLngs.call(this, circle);
        }
    };
    /**
     * Calculate the distance between the current center and an arbitrary position.
     * @param latlng geo-position to calculate distance to
     * @return distance in meters
     */
    GeodesicCircleClass.prototype.distanceTo = function (latlng) {
        var dest = latlngExpressiontoLatLng(latlng);
        return this.geom.distance(this.center, dest);
    };
    /**
     * Set a new center for the geodesic circle and update the geometry. Radius may also be set.
     * @param center the new center
     * @param radius the new radius
     */
    GeodesicCircleClass.prototype.setLatLng = function (center, radius) {
        this.center = latlngExpressiontoLatLng(center);
        this.radius = radius ? radius : this.radius;
        this.update();
    };
    /**
     * Set a new radius for the geodesic circle and update the geometry. Center may also be set.
     * @param radius the new radius
     * @param center the new center
     */
    GeodesicCircleClass.prototype.setRadius = function (radius, center) {
        this.radius = radius;
        this.center = center ? latlngExpressiontoLatLng(center) : this.center;
        this.update();
    };
    return GeodesicCircleClass;
}(L__namespace.Polyline));

if (typeof window.L !== "undefined") {
    window.L.Geodesic = GeodesicLine;
    window.L.geodesic = function () {
        var args = [];
        for (var _i = 0; _i < arguments.length; _i++) {
            args[_i] = arguments[_i];
        }
        return new (GeodesicLine.bind.apply(GeodesicLine, __spreadArray([void 0], args, false)))();
    };
    window.L.GeodesicCircle = GeodesicCircleClass;
    window.L.geodesiccircle = function () {
        var args = [];
        for (var _i = 0; _i < arguments.length; _i++) {
            args[_i] = arguments[_i];
        }
        return new (GeodesicCircleClass.bind.apply(GeodesicCircleClass, __spreadArray([void 0], args, false)))();
    };
}

exports.GeodesicCircleClass = GeodesicCircleClass;
exports.GeodesicLine = GeodesicLine;
