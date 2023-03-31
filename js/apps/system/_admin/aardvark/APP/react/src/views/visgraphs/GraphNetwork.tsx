import { Box, Progress } from "@chakra-ui/react";
import React, { useEffect, useRef, useState } from "react";
import { DataSet } from "vis-data";
import { Network } from "vis-network";
import { useElementPosition } from "../../components/hooks/useElementPosition";
import { useGraph } from "./GraphContext";
import { GraphInfo } from "./GraphInfo";
import { GraphRightClickMenu } from "./GraphRightClickMenu";
import { SmartGraphEmptyState } from "./SmartGraphEmptyState";

let timer: number;

export const GraphNetwork = () => {
  const visJsRef = useRef<HTMLDivElement>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const {
    graphData,
    setNetwork,
    setDatasets,
    onAddEdge,
    onSelectEntity,
    hasDrawnOnce
  } = useGraph();
  const { edges, nodes, settings } = graphData || {};
  const { layout: options } = settings || {};
  const isSmart = graphData?.settings.isSmart || false;
  const [progressValue, setProgressValue] = useState(0);
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

    function registerNetwork() {
      newNetwork.on("stabilizationProgress", function(params) {
        if (nodes?.length && nodes.length > 1) {
          const widthFactor = params.iterations / params.total;
          const calculatedProgressValue = Math.round(widthFactor * 100);
          setProgressValue(calculatedProgressValue);
        }
      });
      newNetwork.on("stabilizationIterationsDone", function() {
        newNetwork.fit();
        newNetwork.setOptions({
          physics: false
        });
      });
      newNetwork.on("startStabilizing", function() {
        if (hasDrawnOnce.current) {
          newNetwork.stopSimulation();
        }
        hasDrawnOnce.current = true;
      });
      newNetwork.on("stabilized", () => {
        clearTimeout(timer);
        setProgressValue(100);

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

    registerNetwork();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edges, nodes, options, setNetwork]);
  const containerPosition = useElementPosition(containerRef);
  const height = containerPosition
    ? `calc(100vh - ${containerPosition.top}px)`
    : "80vh";
  return (
    <Box
      ref={containerRef}
      height={height}
      background="white"
      position="relative"
    >
      {!isSmart ? (
        <GraphNetworkInner
          containerRef={containerRef}
          progressValue={progressValue}
          visJsRef={visJsRef}
        />
      ) : (
        <SmartGraphEmptyState />
      )}
    </Box>
  );
};

const GraphNetworkInner = ({
  containerRef,
  progressValue,
  visJsRef
}: {
  containerRef: React.RefObject<HTMLDivElement>;
  progressValue: number;
  visJsRef: React.RefObject<HTMLDivElement>;
}) => {
  return (
    <>
      <GraphRightClickMenu
        portalProps={{
          containerRef
        }}
      />
      <Box id="graphNetworkWrap" height="full" backgroundColor="white">
        {progressValue < 100 ? (
          <Box
            width="full"
            position="absolute"
            paddingX="10"
            top="50%"
            translateY="-100%"
          >
            <Progress value={progressValue} colorScheme="green" />
          </Box>
        ) : null}
        <Box ref={visJsRef} height="calc(100% - 40px)" width="full" />
        <GraphInfo />
      </Box>
    </>
  );
};
