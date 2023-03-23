import React, { useEffect, useRef } from "react";
import { Network } from "vis-network";
import { useGraph } from "./GraphContext";
import { GraphContextMenu } from "./GraphContextMenu";

export const GraphNetwork = () => {
  const visJsRef = useRef<HTMLDivElement>(null);
  const { graphData, setNetwork } = useGraph();
  useEffect(() => {
    const network =
      visJsRef.current &&
      new Network(
        visJsRef.current,
        { nodes: graphData.nodes, edges: graphData.edges },
        graphData.settings.layout
      );
    network && setNetwork(network);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [graphData.edges, graphData.nodes, graphData.settings.layout]);
  return (
    <div>
      <GraphContextMenu visJsRef={visJsRef} />
      <div
        ref={visJsRef}
        style={{
          height: "90vh",
          width: "97%",
          background: "#fff",
          margin: "auto"
        }}
      />
    </div>
  );
};
