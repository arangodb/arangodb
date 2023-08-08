import { EdgeDataType, NodeDataType } from "../../graphV2/GraphData.types";

type EdgeDataInputType = {
  _id: string;
  _from: string;
  _to: string;
  _key: string;
};

type ObjectDataInputType = {
  vertices: {
    _id: string;
    _key: string;
  }[];
  edges: EdgeDataInputType[];
};

const EDGE_COLOR = "#1D2A12";
const GRAPH_SETTINGS = {
  edges: {
    smooth: { type: "dynamic" },
    arrows: {
      to: {
        enabled: true,
        type: "arrow",
        scaleFactor: 0.5
      }
    }
  }
};

export const convertToGraphData = ({
  data,
  graphDataType
}: {
  data: ObjectDataInputType[] | EdgeDataInputType[];
  graphDataType?: "graphObject" | "edgeArray";
}) => {
  if (graphDataType === "graphObject") {
    const { nodes, edges } = convertGraphObject({
      data: data as ObjectDataInputType[]
    });
    return {
      nodes,
      edges,
      settings: GRAPH_SETTINGS
    };
  } else {
    const { nodes, edges } = convertEdgeArray({
      data: data as EdgeDataInputType[]
    });
    return {
      nodes,
      edges,
      settings: GRAPH_SETTINGS
    };
  }
};

const convertGraphObject = ({ data }: { data: ObjectDataInputType[] }) => {
  let nodes = [] as NodeDataType[];
  let edges = [] as EdgeDataType[];
  let nodeIds = [] as string[];
  let edgeIds = [] as string[];
  data.forEach(function (obj) {
    if (obj.edges && obj.vertices) {
      obj.vertices.forEach(function (node) {
        if (node !== null) {
          const graphNode = createGraphNode({
            nodeId: node._id,
            nodeLabel: node._key
          });
          if (!nodeIds.includes(graphNode.id)) {
            nodes = [...nodes, graphNode];
            nodeIds.push(node._id);
          }
        }
      });
      obj.edges.forEach(function (edge) {
        if (edge !== null) {
          const graphEdge = createGraphEdge(edge);
          if (
            nodeIds.includes(graphEdge.from) &&
            nodeIds.includes(graphEdge.to) &&
            !edgeIds.includes(graphEdge.id)
          ) {
            edges = [...edges, graphEdge];
            edgeIds.push(edge._id);
          }
        }
      });
    }
  });
  return {
    nodes,
    edges
  };
};

const convertEdgeArray = ({ data }: { data: EdgeDataInputType[] }) => {
  let nodes = [] as NodeDataType[];
  let edges = [] as EdgeDataType[];
  let nodeIds = [] as string[];
  let edgeIds = [] as string[];

  data.forEach(edge => {
    if (edge !== null) {
      const graphEdge = createGraphEdge(edge);
      if (!edgeIds.includes(edge._id)) {
        edges = [...edges, graphEdge];
        edgeIds.push(edge._id);
        if (!nodeIds.includes(edge._from)) {
          const graphNode = createGraphNode({
            nodeId: edge._from,
            nodeLabel: edge._from
          });
          nodes = [...nodes, graphNode];
          nodeIds.push(edge._from);
        }
        if (!nodeIds.includes(edge._to)) {
          const graphNode = createGraphNode({
            nodeId: edge._to,
            nodeLabel: edge._to
          });
          nodes = [...nodes, graphNode];
          nodeIds.push(edge._to);
        }
      }
    }
  });
  return {
    nodes,
    edges
  };
};

const createGraphEdge = (edge: EdgeDataInputType) => {
  const graphEdge = {
    id: edge._id,
    to: edge._to,
    from: edge._from,
    color: EDGE_COLOR
  };
  return graphEdge;
};

const createGraphNode = ({
  nodeId,
  nodeLabel
}: {
  nodeId: string;
  nodeLabel?: string;
}) => {
  const graphNode = {
    id: nodeId,
    label: nodeLabel || nodeId,
    shape: "dot",
    size: 10
  };
  return graphNode;
};
