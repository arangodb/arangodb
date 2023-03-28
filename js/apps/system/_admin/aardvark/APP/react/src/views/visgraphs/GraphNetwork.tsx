import { Box } from "@chakra-ui/react";
import React, { useEffect, useRef } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { useElementPosition } from "../../components/hooks/useElementPosition";
import { useGraph } from "./GraphContext";
import { GraphInfo } from "./GraphInfo";
import { GraphRightClickMenu } from "./GraphRightClickMenu";

let timer: number;
let hasDrawnOnce = false;
function registerNetwork({
  newNetwork,
  onSelectEntity
}: {
  newNetwork: Network;
  onSelectEntity: (data: { entityId: string; type: "node" | "edge" }) => void;
}) {
  newNetwork.on("stabilizationIterationsDone", function() {
    newNetwork.fit();
    newNetwork.setOptions({
      physics: false
    });
  });
  newNetwork.on("startStabilizing", function() {
    if (hasDrawnOnce) {
      newNetwork.stopSimulation();
    }
    hasDrawnOnce = true;
  });
  newNetwork.on("stabilized", () => {
    clearTimeout(timer);

    timer = setTimeout(() => {
      newNetwork.stopSimulation();
      newNetwork.setOptions({
        physics: false,
        layout: {
          hierarchical: false
        }
      });
    }, 1000);
  });

  newNetwork.on("selectNode", event => {
    if (event.nodes.length === 1) {
      onSelectEntity({ entityId: event.nodes[0], type: "node" });
    }
  });

  newNetwork.on("selectEdge", event => {
    if (event.edges.length === 1) {
      onSelectEntity({ entityId: event.edges[0], type: "edge" });
    }
  });
}

export const GraphNetwork = () => {
  const visJsRef = useRef<HTMLDivElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const {
    graphData,
    setNetwork,
    setDatasets,
    onAddEdge,
    onSelectEntity
  } = useGraph();
  const { edges, nodes, settings } = graphData || {};
  const { layout: options } = settings || {};
  useEffect(() => {
    if (!nodes || !edges || !visJsRef.current) {
      return;
    }
    const nodesDataSet = new DataSet(nodes);
    const edgesDataSet = new DataSet(edges);
    setDatasets({ nodes: nodesDataSet, edges: edgesDataSet });
    const newOptions = {
      ...options,
      manipulation: {
        enabled: false,
        addEdge: function(data: { from: string; to: string }, callback: any) {
          onAddEdge({ data, callback });
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
    setNetwork(newNetwork);
    registerNetwork({ newNetwork, onSelectEntity });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edges, nodes, options, setNetwork]);
  const position = useElementPosition(visJsRef);
  const height = position ? `calc(100vh - ${position.top}px)` : "80vh";
  return (
    <Box
      id="graphNetworkContainer"
      ref={containerRef}
      height={height}
      background="white"
      position="relative"
    >
      <GraphRightClickMenu
        portalProps={{
          containerRef
        }}
      />
      <div
        ref={visJsRef}
        style={{
          height: "calc(100% - 40px)",
          width: "100%",
          background: "#fff",
          margin: "auto"
        }}
      />
      <GraphInfo />
    </Box>
  );
};
