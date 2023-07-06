import React, { useEffect, useRef, useState } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { EdgeDataType, NodeDataType } from "../../graphV2/GraphData.types";

type ObjectDataInputType = {
  vertices: {
    _id: string;
    _key: string;
  }[];
  edges: EdgeDataInputType[];
};
type EdgeDataInputType = {
  _id: string;
  _from: string;
  _to: string;
  _key: string;
};
export const convertToGraphData = ({
  data,
  graphDataType
}: {
  data: ObjectDataInputType[] | EdgeDataInputType[];
  graphDataType?: "graphObject" | "edgeArray";
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
  if (graphDataType === "graphObject") {
    (data as ObjectDataInputType[]).forEach(function (obj) {
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
    (data as EdgeDataInputType[]).forEach(edge => {
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
  }
};
export const useSetupQueryGraph = ({
  visJsRef,
  graphData,
  userSettings
}: {
  graphData?: { edges: EdgeDataType[]; nodes: NodeDataType[]; settings: any };
  visJsRef: React.RefObject<HTMLDivElement>;
  userSettings?: any;
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
    hasDrawnOnce.current = false;
  }, [userSettings?.layout]);
  useEffect(() => {
    let timer = 0;
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
          if (calculatedProgressValue < 20) {
            hasDrawnOnce.current = false;
          }
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
