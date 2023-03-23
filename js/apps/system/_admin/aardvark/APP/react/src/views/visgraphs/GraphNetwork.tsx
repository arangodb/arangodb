import React, { useState, useEffect, useRef } from "react";
import styled from "styled-components";
import { Network } from "vis-network";
import { isEqual } from "lodash";
import { ProgressBar } from "./ProgressBar";
import { Box, Center, Progress } from "@chakra-ui/react";
import { useGraph } from "./GraphContext";

const StyledContextComponent = styled.div`
  position: absolute;
  left: ${props => props.left};
  top: ${props => props.top};
  display: flex;
  flex-direction: column;
  z-index: 99999;
  background-color: rgb(85, 85, 85);
  color: #ffffff;
  border-radius: 10px;
  box-shadow: 0 5px 15px rgb(85 85 85 / 50%);
  padding: 10px 0;
`;

const GraphNetwork = () => {
  const visJsRef = useRef(null);
  const { graphData } = useGraph();
  console.log({ graphData });
  useEffect(() => {
    const network =
      visJsRef.current &&
      new Network(
        visJsRef.current,
        { nodes: graphData.nodes, edges: graphData.edges },
        graphData.settings.layout
      );
    console.log({ network, layout: graphData.settings.layout });
  }, [graphData.edges, graphData.nodes, graphData.settings.layout]);
  return (
    <>
      <div
        id="GraphNetworkdiv"
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

export default GraphNetwork;
