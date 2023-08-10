import React, { useEffect, useState } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import {
  AddEdgeCallback,
  AddEdgeData,
  GraphPointer
} from "../GraphAction.types";
import { useGraph } from "../GraphContext";

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
    setSelectedAction,
    onSelectEntity,
    hasDrawnOnce,
    setRightClickedEntity
  } = useGraph();
  const [progressValue, setProgressValue] = useState(0);

  const { edges, nodes, settings } = graphData || {};
  const { layout: options } = settings || {};
  const onAddEdge = (data: AddEdgeData, callback: AddEdgeCallback) => {
    setSelectedAction(action => {
      if (!action) {
        return;
      }
      return { ...action, from: data.from, to: data.to, callback };
    });
  };
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
        addEdge: onAddEdge
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
    newNetwork.on(
      "oncontext",
      ({ event, pointer }: { event: Event; pointer: GraphPointer }) => {
        event.preventDefault();
        const nodeId = newNetwork.getNodeAt(pointer.DOM) as string;
        const edgeId = newNetwork.getEdgeAt(pointer.DOM) as string;
        if (nodeId) {
          setRightClickedEntity({ type: "node", nodeId, pointer });
          newNetwork.selectNodes([nodeId]);
        } else if (edgeId) {
          setRightClickedEntity({ type: "edge", edgeId, pointer });
          newNetwork.selectEdges([edgeId]);
        } else {
          setRightClickedEntity({ type: "canvas", pointer });
        }
      }
    );
    registerNetwork();
    setNetwork(newNetwork);
    return () => {
      newNetwork?.destroy();
      clearTimeout(timer);
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edges, nodes, options]);
  return { progressValue, setProgressValue };
};
