import { Box, Button, Progress } from "@chakra-ui/react";
import React, { useEffect, useMemo, useRef, useState } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { EdgeDataType, NodeDataType } from "../../graphV2/GraphData.types";
import { QueryResultType } from "../QueryContextProvider";

type ObjectInputDataType = {
  vertices: {
    _id: string;
    _key: string;
  }[];
  edges: {
    _id: string;
    _from: string;
    _to: string;
    _key: string;
  }[];
};
type ArrayDataInputType = {
  _id: string;
  _from: string;
  _to: string;
  _key: string;
};
let timer: number;
const convertToGraphData = ({
  data,
  graphDataType
}: {
  data: ObjectInputDataType[] | ArrayDataInputType[];
  graphDataType: "object" | "array";
}) => {
  const graphSettings = {
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
  let nodes = [] as NodeDataType[];
  let edges = [] as EdgeDataType[];
  const color = "#48bb78";
  let nodeIds = [] as string[];
  let edgeIds = [] as string[];
  if (graphDataType === "object") {
    (data as ObjectInputDataType[]).forEach(function (obj) {
      if (obj.edges && obj.vertices) {
        obj.vertices.forEach(function (node) {
          if (node !== null) {
            const graphNode = {
              id: node._id,
              label: node._key,
              color: color,
              shape: "dot",
              size: 10
            };
            if (!nodeIds.includes(graphNode.id)) {
              nodes = [...nodes, graphNode];
              nodeIds.push(node._id);
            }
          }
        });
        obj.edges.forEach(function (edge) {
          if (edge !== null) {
            const graphEdge = {
              id: edge._id,
              from: edge._from,
              color: "#cccccc",
              to: edge._to
            };
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
      edges,
      settings: graphSettings
    };
  } else {
    (data as ArrayDataInputType[]).forEach(edge => {
      if (edge !== null) {
        const graphEdge = {
          id: edge._id,
          from: edge._from,
          color: "#cccccc",
          to: edge._to
        };
        if (!edgeIds.includes(edge._id)) {
          edges = [...edges, graphEdge];
          edgeIds.push(edge._id);
          if (!nodeIds.includes(edge._from)) {
            const graphNode = {
              id: edge._from,
              label: edge._from,
              color: color,
              shape: "dot",
              size: 10
            };
            nodes = [...nodes, graphNode];
            nodeIds.push(edge._from);
          }
          if (!nodeIds.includes(edge._to)) {
            const graphNode = {
              id: edge._to,
              label: edge._to,
              color: color,
              shape: "dot",
              size: 10
            };
            nodes = [...nodes, graphNode];
            nodeIds.push(edge._to);
          }
        }
      }
    });
    return {
      nodes,
      edges,
      settings: graphSettings
    };
    /**
     * var edgeObj = {};

    _.each(data, function (edge) {
      if (edge) {
        vertices[edge._from] = null;
        vertices[edge._to] = null;

        if (edge._id) {
          edgeObj[edge._id] = {
            id: edge._id,
            source: edge._from,
            color: "#cccccc",
            target: edge._to,
          };
        }
      }
    });

    _.each(vertices, function (val, key) {
      returnObj.nodes.push({
        id: key,
        label: key,
        size: 0.3,
        color: color,
        x: Math.random(),
        y: Math.random(),
      });
    });
    _.each(edgeObj, function (edge) {
      returnObj.edges.push(edge);
    });
     */
  }
};

const useSetupQueryGraph = ({
  visJsRef,
  graphData
}: {
  graphData?: { edges: EdgeDataType[]; nodes: NodeDataType[]; settings: any };
  data: ObjectInputDataType[];
  visJsRef: React.RefObject<HTMLDivElement>;
}) => {
  const [network, setNetwork] = useState<Network>();
  const [datasets, setDatasets] = useState<{
    nodes: DataSet<NodeDataType>;
    edges: DataSet<EdgeDataType>;
  }>();

  const hasDrawnOnce = useRef(false);
  const [progressValue, setProgressValue] = useState(0);

  const { edges, nodes, settings } = graphData || {};
  const options = settings || {};

  useEffect(() => {
    if (!visJsRef.current || !nodes || !edges) {
      return;
    }
    const nodesDataSet = new DataSet(nodes || []);
    const edgesDataSet = new DataSet(edges || []);
    setDatasets({ nodes: nodesDataSet, edges: edgesDataSet });
    const newOptions = {
      ...options,
      manipulation: {
        enabled: false
      },
      physics: {
        ...options?.physics,
        stabilization: {
          enabled: true,
          iterations: 500,
          updateInterval: 25
        }
      }
    };
    const newNetwork = new Network(
      visJsRef.current,
      {
        nodes: nodesDataSet,
        edges: edgesDataSet
      },
      newOptions
    );

    function registerNetwork() {
      newNetwork.on("stabilizationProgress", function (params) {
        if (nodes?.length && nodes.length > 1) {
          const widthFactor = params.iterations / params.total;
          const calculatedProgressValue = Math.round(widthFactor * 100);
          setProgressValue(calculatedProgressValue);
        }
      });
      newNetwork.on("startStabilizing", function () {
        if (hasDrawnOnce.current) {
          newNetwork.stopSimulation();
        }
        hasDrawnOnce.current = true;
      });
      newNetwork.on("stabilizationIterationsDone", function () {
        newNetwork.fit();
        newNetwork.setOptions({
          physics: false
        });
      });
      newNetwork.on("stabilized", () => {
        clearTimeout(timer);
        setProgressValue(100);

        timer = window.setTimeout(() => {
          newNetwork.stopSimulation();
          newNetwork.setOptions({
            physics: false,
            layout: {
              hierarchical: false
            }
          });
        }, 1000);
      });
    }
    registerNetwork();
    setNetwork(newNetwork);
    return () => {
      newNetwork?.destroy();
      clearTimeout(timer);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edges, nodes, options]);
  return { progressValue, network, datasets };
};
export const QueryGraphView = ({
  queryResult,
  graphDataType
}: {
  queryResult: QueryResultType;
  graphDataType: "object" | "array";
}) => {
  const graphData = useMemo(() => {
    return convertToGraphData({ graphDataType, data: queryResult.result });
  }, [queryResult.result, graphDataType]);
  const visJsRef = useRef<HTMLDivElement>(null);
  const { progressValue, network } = useSetupQueryGraph({
    visJsRef,
    graphData,
    data: queryResult.result
  });
  return (
    <Box position="relative">
      {progressValue < 100 && (
        <Box marginX="10" marginY="10">
          <Progress value={progressValue} colorScheme="green" />
        </Box>
      )}
      <Button
        position="absolute"
        zIndex="1"
        right="12px"
        top="12px"
        size="xs"
        variant="ghost"
        onClick={() => {
          network?.fit();
        }}
      >
        Reset zoom
      </Button>
      <Box height="500px" width="full" ref={visJsRef} />
    </Box>
  );
};
