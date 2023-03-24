import React from "react";
import { GraphContextProvider } from "./GraphContext";
import { GraphNetwork } from "./GraphNetwork";
import { GraphHeader } from "./GraphHeader";
import { GraphModals } from "./GraphModals";

export const GraphDisplay = () => {
  return (
    <GraphContextProvider>
      <GraphContent />
    </GraphContextProvider>
  );
};

const GraphContent = () => {
  return (
    <>
      <GraphHeader />
      <GraphNetwork />
      <GraphModals />
    </>
  );
};
