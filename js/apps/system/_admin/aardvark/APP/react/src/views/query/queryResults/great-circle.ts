import type { Geometry, Position } from "geojson";
import { GreatCircle } from "arc";

// Great-circle interpolation, so a line between two far-apart points follows
// the shortest path over the globe instead of a straight line on the flat
// projection. This replaces what leaflet.geodesic used to do, using `arc`
// (BSD-2-Clause, no dependencies).

const POINTS_PER_SEGMENT = 65;

const toCoord = (position: Position) => ({ x: position[0], y: position[1] });

const isFinitePosition = (position: Position) =>
  Number.isFinite(position[0]) && Number.isFinite(position[1]);

// A segment arc cannot describe falls back to the straight line between its
// endpoints, so one bad segment never costs the rest of the geometry. arc
// throws on exactly antipodal endpoints (pole to pole, or [0,0] to [180,0]),
// where there are infinitely many great circles and a straight line is as
// good as any, and returns NaN for a zero-length segment.
const arcPieces = (from: Position, to: Position): Position[][] => {
  try {
    const { geometry } = new GreatCircle(toCoord(from), toCoord(to))
      .Arc(POINTS_PER_SEGMENT)
      .json();
    const pieces =
      geometry.type === "MultiLineString"
        ? geometry.coordinates
        : geometry.type === "LineString"
        ? [geometry.coordinates]
        : null;
    if (pieces?.every(piece => piece.every(isFinitePosition))) {
      return pieces;
    }
  } catch {
    // fall through to the straight segment
  }
  return [[from, to]];
};

// the same fallback for rings, which are densified but never split
const interpolateSegment = (from: Position, to: Position): Position[] => {
  try {
    const circle = new GreatCircle(toCoord(from), toCoord(to));
    const points: Position[] = [];
    for (let step = 0; step < POINTS_PER_SEGMENT; step++) {
      points.push(circle.interpolate(step / (POINTS_PER_SEGMENT - 1)));
    }
    if (points.every(isFinitePosition)) {
      return points;
    }
  } catch {
    // fall through to the straight segment
  }
  return [from, to];
};

// walks a line, keeping the splits arc made: a new piece joins the previous
// one only where they meet, otherwise it starts a fresh piece.
const densifyLine = (line: Position[]): Position[][] => {
  if (line.length < 2) {
    return [line];
  }
  const pieces: Position[][] = [];
  for (let index = 0; index < line.length - 1; index++) {
    for (const piece of arcPieces(line[index], line[index + 1])) {
      const previous = pieces[pieces.length - 1];
      const joins =
        previous &&
        previous[previous.length - 1][0] === piece[0][0] &&
        previous[previous.length - 1][1] === piece[0][1];
      if (joins) {
        previous.push(...piece.slice(1));
      } else {
        pieces.push([...piece]);
      }
    }
  }
  return pieces;
};

// Rings are densified but never split. Cutting one would break the polygon,
// so a polygon spanning the antimeridian renders wrong, exactly as it did
// under leaflet.geodesic.
const densifyRing = (ring: Position[]): Position[] => {
  if (ring.length < 2) {
    return ring;
  }
  const densified: Position[] = [];
  for (let index = 0; index < ring.length - 1; index++) {
    const segment = interpolateSegment(ring[index], ring[index + 1]);
    densified.push(...(index === 0 ? segment : segment.slice(1)));
  }
  return densified;
};

// replaces every straight segment of a geometry with its great-circle arc.
// points are returned untouched.
export const toGreatCircle = (geometry: Geometry): Geometry => {
  switch (geometry.type) {
    case "LineString": {
      const pieces = densifyLine(geometry.coordinates);
      return pieces.length === 1
        ? { type: "LineString", coordinates: pieces[0] }
        : { type: "MultiLineString", coordinates: pieces };
    }
    case "MultiLineString":
      return {
        ...geometry,
        coordinates: geometry.coordinates.flatMap(densifyLine)
      };
    case "Polygon":
      return {
        ...geometry,
        coordinates: geometry.coordinates.map(densifyRing)
      };
    case "MultiPolygon":
      return {
        ...geometry,
        coordinates: geometry.coordinates.map(polygon =>
          polygon.map(densifyRing)
        )
      };
    case "GeometryCollection":
      return {
        ...geometry,
        geometries: geometry.geometries.map(toGreatCircle)
      };
    default:
      return geometry;
  }
};
