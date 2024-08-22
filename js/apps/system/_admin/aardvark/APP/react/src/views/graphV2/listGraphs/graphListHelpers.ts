import { EdgeDefinition, GraphInfo } from "arangojs/graph";
import * as Yup from "yup";
import { getCurrentDB } from "../../../utils/arangoClient";
import { notifyError, notifySuccess } from "../../../utils/notifications";
import { getNormalizedByteLengthTest } from "../../../utils/yupHelper";
import {
  AllGraphCreateValues,
  GeneralGraphCreateValues
} from "../addGraph/CreateGraph.types";

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
    const response = await currentDB.route().post("/_api/gharial", {
      ...values,
      name: values.name.normalize()
    });
    notifySuccess(`Successfully created the graph: ${values.name}`);
    onSuccess();
    return response;
  } catch (e: any) {
    const errorMessage =
      e?.response?.parsedBody?.errorMessage || "Unknown error";
    notifyError(`Could not create graph: ${errorMessage}`);
    return {
      error: errorMessage
    };
  }
};

type DetectedGraphType = "satellite" | "enterprise" | "smart" | "general";

const GRAPH_TYPE_TO_LABEL_MAP = {
  satellite: "Satellite",
  smart: "Smart",
  enterprise: "Enterprise",
  general: "General"
};

const getGraphLabel = ({
  type,
  isDisjoint
}: {
  type: DetectedGraphType;
  isDisjoint?: boolean;
}) => {
  let graphTypeString = GRAPH_TYPE_TO_LABEL_MAP[type];
  if (isDisjoint) {
    return `Disjoint ${graphTypeString}`;
  }
  return graphTypeString;
};

export const detectGraphType = (
  graph: GraphInfo
): {
  type: DetectedGraphType;
  isDisjoint?: boolean;
  label: string;
} => {
  if (graph.isSatellite || (graph.replicationFactor as any) === "satellite") {
    return {
      type: "satellite",
      label: getGraphLabel({ type: "satellite" })
    };
  }

  if (graph.isSmart && !graph.smartGraphAttribute) {
    return {
      type: "enterprise",
      isDisjoint: graph.isDisjoint,
      label: getGraphLabel({ type: "enterprise", isDisjoint: graph.isDisjoint })
    };
  }

  if (graph.isSmart && !!graph.smartGraphAttribute) {
    return {
      type: "smart",
      isDisjoint: graph.isDisjoint,
      label: getGraphLabel({ type: "smart", isDisjoint: graph.isDisjoint })
    };
  }
  return {
    type: "general",
    label: getGraphLabel({ type: "general" })
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

const documentKeyRegex = window.arangoValidationHelper.getDocumentKeyRegex();
export const GRAPH_VALIDATION_SCHEMA = Yup.object({
  name: Yup.string()
    .required("Name is required")
    .test(
      "normalizedByteLength",
      "Graph name max length is 254 bytes.",
      getNormalizedByteLengthTest(254, "Graph name max length is 254 bytes.")
    )
    .matches(
      documentKeyRegex,
      "Only these characters are allowed: a-z, A-Z, 0-9 and  _ - : . @ ( ) + , = ; $ ! * ' %"
    )
});

/**
 * Handles updates for
 * - Orphans
 * - Edge definitions
 * - Satellite collections
 */
export const updateGraph = async ({
  values,
  onSuccess,
  initialGraph
}: {
  values: AllGraphCreateValues;
  onSuccess: () => void;
  initialGraph?: GraphInfo;
}) => {
  try {
    await updateOrphans({
      values,
      initialGraph
    });
  } catch (e: any) {
    const errorMessage =
      e?.response?.parsedBody?.errorMessage || "Unknown error";
    notifyError(`Could not update orphans: ${errorMessage}`);
    return;
  }
  try {
    await updateEdgeDefinitions({
      values,
      initialGraph
    });
  } catch (e: any) {
    const errorMessage =
      e?.response?.parsedBody?.errorMessage || "Unknown error";
    notifyError(`Could not update edge definitions: ${errorMessage}`);
    return;
  }
  notifySuccess(`Successfully updated the graph: ${values.name}`);
  onSuccess();
};

const updateOrphans = async ({
  values,
  initialGraph
}: {
  values: GeneralGraphCreateValues;
  initialGraph?: GraphInfo;
}) => {
  if (initialGraph?.orphanCollections && values.orphanCollections) {
    const addedOrphanCollections = values.orphanCollections?.filter(
      (collection: string) =>
        !initialGraph.orphanCollections.includes(collection)
    );
    const removedOrphanColletions = initialGraph.orphanCollections.filter(
      (collection: string) =>
        !values.orphanCollections?.includes(collection) &&
        !addedOrphanCollections.includes(collection)
    );
    for (const collection of addedOrphanCollections) {
      await addOrphanCollection({
        collectionName: collection,
        graphName: values.name
      });
    }
    for (const collection of removedOrphanColletions) {
      await removeOrphanCollection({
        collectionName: collection,
        graphName: values.name
      });
    }
  }
};

const removeOrphanCollection = async ({
  collectionName,
  graphName
}: {
  collectionName: string;
  graphName: string;
}) => {
  const currentDB = getCurrentDB();
  const graph = currentDB.graph(graphName);
  return graph.removeVertexCollection(collectionName);
};

const addOrphanCollection = async ({
  collectionName,
  graphName
}: {
  collectionName: string;
  graphName: string;
}) => {
  const currentDB = getCurrentDB();
  const graph = currentDB.graph(graphName);
  return graph.addVertexCollection(collectionName);
};

const updateEdgeDefinitions = async ({
  values,
  initialGraph
}: {
  values: GeneralGraphCreateValues;
  initialGraph?: GraphInfo;
}) => {
  if (initialGraph?.edgeDefinitions) {
    const result = values.edgeDefinitions.reduce(
      (
        acc: {
          addedEdgeDefinitions: EdgeDefinition[];
          modifiedEdgeDefinitions: EdgeDefinition[];
        },
        edgeDefinition
      ) => {
        // find the current edge definition in the initial graph
        const initialEdgeDefinition = initialGraph.edgeDefinitions.find(
          initialEdgeDefinition =>
            initialEdgeDefinition.collection === edgeDefinition.collection
        );

        if (!initialEdgeDefinition) {
          acc.addedEdgeDefinitions.push(edgeDefinition);
        } else if (
          initialEdgeDefinition.collection === edgeDefinition.collection &&
          (initialEdgeDefinition.from !== edgeDefinition.from ||
            initialEdgeDefinition.to !== edgeDefinition.to)
        ) {
          // same collection but different from/to
          acc.modifiedEdgeDefinitions.push(edgeDefinition);
        }
        return acc;
      },
      {
        addedEdgeDefinitions: [],
        modifiedEdgeDefinitions: []
      }
    );
    const removedEdgeDefinitions = initialGraph.edgeDefinitions.filter(
      initialEdgeDefinition =>
        !values.edgeDefinitions.find(
          edgeDefinition =>
            edgeDefinition.collection === initialEdgeDefinition.collection
        )
    );
    const satellites = (values as any).options?.satellites as
      | string[]
      | undefined;
    for (const edgeDefinition of result.addedEdgeDefinitions) {
      await addEdgeDefinition({
        edgeDefinition,
        graphName: values.name,
        satellites
      });
    }
    for (const edgeDefinition of removedEdgeDefinitions) {
      await removeEdgeDefinition({
        edgeDefinition,
        graphName: values.name
      });
    }
    for (const edgeDefinition of result.modifiedEdgeDefinitions) {
      await modifyEdgeDefinition({
        edgeDefinition,
        graphName: values.name,
        satellites
      });
    }
  }
};

const addEdgeDefinition = async ({
  edgeDefinition,
  graphName,
  satellites
}: {
  edgeDefinition: EdgeDefinition;
  graphName: string;
  satellites?: string[];
}) => {
  const currentDB = getCurrentDB();
  const graph = currentDB.graph(graphName);
  await graph.addEdgeDefinition(edgeDefinition, {
    satellites
  });
};

const removeEdgeDefinition = async ({
  edgeDefinition,
  graphName
}: {
  edgeDefinition: EdgeDefinition;
  graphName: string;
}) => {
  const currentDB = getCurrentDB();
  const graph = currentDB.graph(graphName);
  await graph.removeEdgeDefinition(edgeDefinition.collection);
};

const modifyEdgeDefinition = async ({
  edgeDefinition,
  graphName,
  satellites
}: {
  edgeDefinition: EdgeDefinition;
  graphName: string;
  satellites?: string[];
}) => {
  const currentDB = getCurrentDB();
  const graph = currentDB.graph(graphName);
  await graph.replaceEdgeDefinition(edgeDefinition.collection, edgeDefinition, {
    satellites
  });
};
