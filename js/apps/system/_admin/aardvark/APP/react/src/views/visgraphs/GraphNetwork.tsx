import React, { useEffect, useRef } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { useGraph } from "./GraphContext";
import { GraphRightClickMenu } from "./GraphRightClickMenu";

let timer: number;
let hasDrawnOnce = false;
function registerNetwork(newNetwork: Network) {
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
}

export const GraphNetwork = () => {
  const visJsRef = useRef<HTMLDivElement>(null);
  const { graphData, setNetwork } = useGraph();
  const { edges, nodes, settings } = graphData || {};
  const { layout: options } = settings || {};
  useEffect(() => {
    if (!nodes || !edges || !visJsRef.current) {
      return;
    }
    const nodesDataSet = new DataSet(nodes);
    const edgesDataSet = new DataSet(edges);
    const newNetwork = new Network(
      visJsRef.current,
      {
        nodes: nodesDataSet,
        edges: edgesDataSet
      },
      options
    );
    setNetwork(newNetwork);
    registerNetwork(newNetwork);
  }, [edges, nodes, options, setNetwork]);

  return (
    <>
      <GraphRightClickMenu visJsRef={visJsRef} />
      <div
        ref={visJsRef}
        style={{
          height: "90vh",
          width: "97%",
          background: "#fff",
          margin: "auto"
        }}
      />
    </>
  );
};
