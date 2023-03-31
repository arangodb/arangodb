import React from "react";
import { GraphContextProvider } from "./GraphContext";
import { GraphCustomStyleReset } from "./GraphCustomStyleReset";
import { GraphHeader } from "./GraphHeader";
import { GraphModals } from "./graphModals/GraphModals";
import { GraphNetwork } from "./GraphNetwork";

export const GraphDisplay = () => {
  return (
    <GraphContextProvider>
      <GraphContent />
    </GraphContextProvider>
  );
};

const GraphContent = () => {
  return (
    <GraphCustomStyleReset>
      <GraphHeader />
      <GraphNetwork />
      <GraphModals />
    </GraphCustomStyleReset>
  );
};
