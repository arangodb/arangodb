import { GraphInfo } from "arangojs/graph";
import { getCurrentDB } from "../../../utils/arangoClient";
import { notifyError, notifySuccess } from "../../../utils/notifications";
import { GraphTypes } from "./Graphs.types";

export const TYPE_TO_LABEL_MAP: {
  [key in GraphTypes]: string;
} = {
  smart: "Smart",
  enterprise: "Enterprise"
};

export const createGraph = async <
  ValuesType extends {
    name: string;
  }
>({
  values,
  onSuccess
}: {
  values: ValuesType;
  onSuccess: () => void;
}) => {
  const currentDB = getCurrentDB();
  try {
    const response = await currentDB.request(
      {
        method: "POST",
        path: "/_api/gharial",
        body: {
          ...values
        }
      },
      res => res.body.graph
    );
    notifySuccess(`Successfully created the graph: ${values.name}`);
    onSuccess();
    return response;
  } catch (e: any) {
    const errorMessage = e.response.body.errorMessage;
    notifyError(`Could not add graph: ${errorMessage}`);
    return {
      error: errorMessage
    };
  }
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

export const GENERAL_GRAPH_FIELDS_MAP = {
  name: {
    name: "name",
    type: "text",
    label: "Name",
    tooltip:
      "String value. The name to identify the graph. Has to be unique and must follow the Document Keys naming conventions.",
    isRequired: true
  },
  orphanCollections: {
    name: "orphanCollections",
    type: "creatableMultiSelect",
    label: "Orphan collections",
    tooltip:
      "Collections that are part of a graph but not used in an edge definition.",
    isRequired: false,
    noOptionsMessage: () => "Please enter a new and valid collection name"
  }
};

export const CLUSTER_GRAPH_FIELDS_MAP = {
  numberOfShards: {
    name: "numberOfShards",
    type: "number",
    label: "Shards",
    tooltip:
      "The number of shards that is used for every collection within this graph. Cannot be modified later. Must be at least 1.",
    isRequired: true
  },
  replicationFactor: {
    name: "replicationFactor",
    type: "number",
    label: "Replication factor",
    tooltip:
      "Total number of copies of the data in the cluster. If not given, the system default for new collections will be used. Must be at least 1.",
    isRequired: false
  },
  writeConcern: {
    name: "writeConcern",
    type: "number",
    label: "Write concern",
    tooltip:
      "Numeric value. Must be at least 1. Must be smaller or equal compared to the replication factor. Total number of copies of the data in the cluster that are required for each write operation. If we get below this value, the collection will be read-only until enough copies are created. If not given, the system default for new collections will be used.",
    isRequired: false
  },
  satellites: {
    name: "satellites",
    type: "creatableMultiSelect",
    label: "Satellite collections",
    tooltip:
      "Insert vertex collections here which are being used in new edge definitions (fromCollections, toCollections). Those defined collections will be created as SatelliteCollections, and therefore will be replicated to all DB-Servers.",
    placeholder: "Insert list of <from>/<to> Vertex Collections"
  }
};

export const SMART_GRAPH_FIELDS_MAP = {
  isDisjoint: {
    name: "isDisjoint",
    type: "boolean",
    label: "Create disjoint graph",
    tooltip:
      "Creates a Disjoint SmartGraph. Creating edges between different SmartGraph components is not allowed.",
    isRequired: false
  },
  smartGraphAttribute: {
    name: "smartGraphAttribute",
    type: "text",
    label: "SmartGraph Attribute",
    tooltip:
      "String value. The attribute name that is used to smartly shard the vertices of a graph. Every vertex in this Graph has to have this attribute. Cannot be modified later.",
    isRequired: true
  }
};
