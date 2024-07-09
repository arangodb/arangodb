import L from "leaflet";
import { GeodesicLine } from "leaflet.geodesic";
import React from "react";
import { QueryResultType } from "../ArangoQuery.types";

export type DisplayType = "json" | "graph" | "table" | "geo";
type GraphDataType = "graphObject" | "edgeArray";
/**
 * check if result could be displayed as graph
 * case a) result has keys named vertices and edges
 * case b) 95% results _from and _to attribute
 */

export const useDisplayTypes = ({
  queryResult
}: {
  queryResult: QueryResultType;
}) => {
  const [displayTypes, setDisplayTypes] = React.useState<Array<DisplayType>>(
    []
  );
  const [currentDisplayType, setCurrentDisplayType] =
    React.useState<DisplayType>("json");
  const [graphDataType, setGraphDataType] =
    React.useState<GraphDataType>("graphObject");
  React.useEffect(() => {
    const { isGraph, graphDataType } = detectGraph({
      result: queryResult.result
    });
    if (isGraph && graphDataType) {
      setDisplayTypes(["graph"]);
      setGraphDataType(graphDataType);
      setCurrentDisplayType(prevView =>
        prevView !== "graph" ? "graph" : prevView
      );
      return;
    }
    const { isGeo, isTable } = detectGeo({
      result: queryResult.result
    });
    if (isGeo) {
      let displayTypes = ["geo"];
      if (isTable) {
        displayTypes = ["table", ...displayTypes];
      }
      setDisplayTypes(displayTypes as DisplayType[]);
      setCurrentDisplayType(prevView =>
        prevView !== "geo" ? "geo" : prevView
      );
    } else if (isTable) {
      setDisplayTypes(["table"]);
      setCurrentDisplayType(prevView =>
        prevView !== "table" ? "table" : prevView
      );
    }
  }, [queryResult]);
  return {
    displayTypes,
    graphDataType,
    currentDisplayType,
    setCurrentDisplayType
  };
};

type EdgeGraphType = {
  _id: string;
  _from: string;
  _to: string;
  _key: string;
};

type VertexGraphType = {
  _id: string;
  _key: string;
};

type ObjectGraphType = {
  vertices: VertexGraphType[];
  edges: EdgeGraphType[];
};
export const detectGraph = ({
  result
}: {
  result: (EdgeGraphType | ObjectGraphType)[];
}): {
  isGraph: boolean;
  graphDataType?: GraphDataType;
} => {
  if (!result || !Array.isArray(result)) {
    return {
      isGraph: false
    };
  }
  const firstValidIndex = result.findIndex(item => {
    return !!item;
  });
  const firstValidItem = result[firstValidIndex];
  const { vertices, edges } = (firstValidItem as ObjectGraphType) || {};
  /**
   * case 1 - it's an array of objects,
   * with keys 'vertices' & 'edges'
   */
  if (vertices && edges) {
    let totalValidEdges = 0;
    let totalEdges = 0;
    (result as ObjectGraphType[]).forEach(obj => {
      if (obj.edges) {
        obj.edges.forEach(edge => {
          if (edge) {
            if (edge._from && edge._to) {
              totalValidEdges++;
            }
            totalEdges++;
          }
        });
      }
    });
    const validEdgesPercentage =
      totalEdges > 0 ? (totalValidEdges / totalEdges) * 100 : 0;
    if (validEdgesPercentage >= 95) {
      return {
        isGraph: true,
        graphDataType: "graphObject"
      };
    }
    return {
      isGraph: false
    };
  } else {
    /**
     * case 2 - it's an array of objects,
     * with keys '_from' and '_to'
     */
    let totalValidEdgeObjects = 0;
    const totalEdgeObjects = result.length;
    (result as EdgeGraphType[]).forEach(edgeObj => {
      if (edgeObj) {
        if (edgeObj._from && edgeObj._to) {
          totalValidEdgeObjects++;
        }
      }
    });

    const validEdgeObjectsPercentage =
      totalEdgeObjects > 0
        ? (totalValidEdgeObjects / totalEdgeObjects) * 100
        : 0;

    if (validEdgeObjectsPercentage >= 95) {
      return {
        isGraph: true,
        graphDataType: "edgeArray"
      };
    }
    return {
      isGraph: false
    };
  }
};

const GEOMETRY_TYPES = [
  "Point",
  "MultiPoint",
  "LineString",
  "MultiLineString",
  "Polygon",
  "MultiPolygon"
];

const POINT_GEOMETRY_TYPES = ["Point", "MultiPoint"];

const GEODESIC_GEOMETRY_TYPES = [
  "LineString",
  "MultiLineString",
  "Polygon",
  "MultiPolygon"
];
type GeoItemType = {
  type?: string;
  coordinates?: any[];
  geometry?: {
    type: string;
    coordinates: any[];
  };
};

const isValidPoint = (item: { type?: string; coordinates?: any[] }) => {
  if (item.type && POINT_GEOMETRY_TYPES.includes(item.type)) {
    try {
      new L.GeoJSON(item as any);
      return true;
    } catch (ignore) {
      return false;
    }
  }
};
const isValidGeodesic = (item: GeoItemType) => {
  if (item.type && GEODESIC_GEOMETRY_TYPES.includes(item.type)) {
    try {
      new GeodesicLine().fromGeoJson(item as any);
      return true;
    } catch (ignore) {
      return false;
    }
  }
};
const detectGeo = ({
  result
}: {
  result: GeoItemType[];
}): {
  isGeo: boolean;
  isTable: boolean;
} => {
  let validGeojsonCount = 0;
  let isGeo = false;
  let isTable = false;
  // makes a map like {type: 1, coordinates: 2} across all resultItems
  let attributeCountMap = {} as {
    [key: string]: number;
  };
  result?.forEach(resultItem => {
    if (
      typeof resultItem !== "object" ||
      resultItem === null ||
      Array.isArray(resultItem)
    ) {
      return {
        isGeo: false,
        isTable: false
      };
    }
    if (resultItem.coordinates && resultItem.type) {
      if (GEOMETRY_TYPES.includes(resultItem.type)) {
        if (isValidPoint(resultItem) || isValidGeodesic(resultItem)) {
          validGeojsonCount++;
        }
      }
    } else if (
      resultItem.geometry &&
      resultItem.geometry.coordinates &&
      resultItem.geometry.type
    ) {
      if (GEOMETRY_TYPES.includes(resultItem.geometry.type)) {
        if (
          isValidPoint(resultItem.geometry) ||
          isValidGeodesic(resultItem.geometry)
        ) {
          validGeojsonCount++;
        }
      }
    }

    const resultItemKeys = Object.keys(resultItem);
    resultItemKeys.forEach(key => {
      if (attributeCountMap.hasOwnProperty(key)) {
        attributeCountMap[key] = attributeCountMap[key] + 1;
      } else {
        attributeCountMap[key] = 1;
      }
    });
  });

  // answers the question - what % of result resultItems have a given attribute
  // for eg, do 95% have attribute 'type'?
  // If yes, then it's can be displayed as a table
  Object.keys(attributeCountMap).forEach(key => {
    const attributeCount = attributeCountMap[key];
    const attributeRateForResultItem = (attributeCount / result.length) * 100;
    if (attributeRateForResultItem <= 95) {
      isTable = false;
    } else {
      isTable = true;
    }
  });

  if (result?.length === validGeojsonCount) {
    isGeo = true;
  } else {
    isGeo = false;
    isTable = true;
  }
  return {
    isGeo,
    isTable
  };
};
