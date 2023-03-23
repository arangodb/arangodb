import React from "react";
import { GraphContextProvider, useGraph } from "./GraphContext";
import { GraphNetwork } from "./GraphNetwork";
import { GraphHeader } from "./GraphHeader";

export const GraphDisplay = () => {
  return (
    <GraphContextProvider>
      <GraphContent />
    </GraphContextProvider>
  );
};

const GraphContent = () => {
  const { graphData } = useGraph();
  if (!graphData) {
    return <>Graph not found</>;
  }
  return (
    <>
      <GraphHeader />
      <GraphNetwork />
    </>
  );
};
