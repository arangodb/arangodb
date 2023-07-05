import { GraphInfo } from "arangojs/graph";
import { getCurrentDB } from "../../utils/arangoClient";
import { SmartGraphCreateValues } from "./addGraph/CreateGraph.types";
import { GraphTypes } from "./Graphs.types";

export const TYPE_TO_LABEL_MAP: {
  [key in GraphTypes]: string;
} = {
  smart: "Smart",
  enterprise: "Enterprise"
};

export const createGraph = async (values: SmartGraphCreateValues) => {
  const currentDB = getCurrentDB();
  const { orphanCollections, edgeDefinitions, isSmart, name, ...options } =
    values;
  return currentDB.request(
    {
      method: "POST",
      path: "/_api/gharial",
      body: {
        orphanCollections,
        edgeDefinitions,
        isSmart,
        name,
        options
      }
    },
    res => res.body.graph
  );
};

type DetectedGraphType = "satellite" | "enterprise" | "smart" | "general";

export const detectType = (
  graph: GraphInfo
): {
  type: DetectedGraphType;
  isDisjoint?: boolean;
} => {
  if (graph.isSatellite || (graph.replicationFactor as any) === "satellite") {
    return {
      type: "satellite"
    };
  }

  if (graph.isSmart && graph.smartGraphAttribute === undefined) {
    return {
      type: "enterprise",
      isDisjoint: graph.isDisjoint
    };
  }

  if (graph.isSmart === true && graph.smartGraphAttribute !== undefined) {
    return {
      type: "smart",
      isDisjoint: graph.isDisjoint
    };
  }
  return {
    type: "general"
  };
};
