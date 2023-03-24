import React, { useEffect, useRef } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { useGraph } from "./GraphContext";
import { GraphContextMenu } from "./GraphContextMenu";

export const GraphNetwork = () => {
  const visJsRef = useRef<HTMLDivElement>(null);
  const { graphData, network, setNetwork } = useGraph();
  const { edges, nodes, settings } = graphData || {};
  const { layout } = settings || {};
  useEffect(() => {
    if (!nodes || !edges || network) {
      return;
    }
    const nodesDataSet = new DataSet(nodes);
    const edgesDataSet = new DataSet(edges);
    const newNetwork =
      visJsRef.current &&
      new Network(
        visJsRef.current,
        {
          nodes: nodesDataSet,
          edges: edgesDataSet
        },
        layout
      );
    newNetwork && setNetwork(newNetwork);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edges, nodes, network]);
  useEffect(() => {
    if (!nodes || !edges) {
      return;
    }
    const nodesDataSet = new DataSet(nodes);
    const edgesDataSet = new DataSet(edges);
    network && network.setData({ nodes: nodesDataSet, edges: edgesDataSet });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [nodes, edges]);
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
