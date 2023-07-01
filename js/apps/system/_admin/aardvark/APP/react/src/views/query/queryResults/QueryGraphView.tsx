import { Box } from "@chakra-ui/react";
import React, { useEffect, useMemo, useRef, useState } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { EdgeDataType, NodeDataType } from "../../graphV2/GraphData.types";
import { QueryResultType } from "../QueryContextProvider";

type InputDataType = {
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
let timer: number;
const convertToGraphData = ({ data }: { data: InputDataType[] }) => {
  let nodes = [] as NodeDataType[];
  let edges = [] as EdgeDataType[];
  const color = "#48bb78";
  const type = "object";
  let nodeIds = [] as string[];
  let edgeIds = [] as string[];
  if (type === "object") {
    data.forEach(function (obj) {
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
      settings: {
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
      }
    };
  }
};

const useSetupQueryGraph = ({
  visJsRef,
  graphData
}: {
  graphData?: { edges: EdgeDataType[]; nodes: NodeDataType[]; settings: any };
  data: InputDataType[];
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
  const { layout: options } = settings || {};

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
  queryResult
}: {
  queryResult: QueryResultType;
}) => {
  const graphData = useMemo(() => {
    return convertToGraphData({ data: queryResult.result });
  }, [queryResult.result]);
  const visJsRef = useRef<HTMLDivElement>(null);
  const { progressValue } = useSetupQueryGraph({
    visJsRef,
    graphData,
    data: queryResult.result
  });
  return (
    <>
      {progressValue < 100 ? <div>{progressValue}%</div> : null}
      <Box height="500px" width="full" ref={visJsRef} />
    </>
  );
};
