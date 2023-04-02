import React, { useEffect, useState } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { useGraph } from "./GraphContext";

let timer: number;
export const useSetupGraphNetwork = ({
  visJsRef
}: {
  visJsRef: React.RefObject<HTMLDivElement>;
}) => {
  const {
    graphData,
    setNetwork,
    setDatasets,
    onAddEdge,
    onSelectEntity,
    hasDrawnOnce
  } = useGraph();
  const [progressValue, setProgressValue] = useState(0);

  const { edges, nodes, settings } = graphData || {};
  const { layout: options } = settings || {};

  useEffect(() => {
    if (!visJsRef.current) {
      return;
    }
    const nodesDataSet = new DataSet(nodes || []);
    const edgesDataSet = new DataSet(edges || []);
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

    function registerNetwork() {
      newNetwork.on("stabilizationProgress", function(params) {
        if (nodes?.length && nodes.length > 1) {
          const widthFactor = params.iterations / params.total;
          const calculatedProgressValue = Math.round(widthFactor * 100);
          setProgressValue(calculatedProgressValue);
        }
      });
      newNetwork.on("startStabilizing", function() {
        if (hasDrawnOnce.current) {
          newNetwork.stopSimulation();
        }
        hasDrawnOnce.current = true;
      });
      newNetwork.on("stabilizationIterationsDone", function() {
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
    registerNetwork();

    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edges, nodes, options, setNetwork]);
  return { progressValue, setProgressValue };
};
