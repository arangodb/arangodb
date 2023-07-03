import React from "react";
import { QueryResultType } from "../QueryContextProvider";

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
  const [currentView, setCurrentView] = React.useState<DisplayType>("json");
  const [graphDataType, setGraphDataType] =
    React.useState<GraphDataType>("graphObject");
  React.useEffect(() => {
    const { isGraph, graphDataType } = detectGraph({
      result: queryResult.result
    });
    if (isGraph && graphDataType) {
      setDisplayTypes(["graph"]);
      setGraphDataType(graphDataType as GraphDataType);
      setCurrentView(prevView => {
        if (prevView !== "graph") {
          return "graph";
        }
        return prevView;
      });
    }
  }, [queryResult]);
  return { displayTypes, graphDataType, currentView, setCurrentView };
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
const detectGraph = ({
  result
}: {
  result: (EdgeGraphType | ObjectGraphType)[];
}) => {
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
          if (edge !== null) {
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
