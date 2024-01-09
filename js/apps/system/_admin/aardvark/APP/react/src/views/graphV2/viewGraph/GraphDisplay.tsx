import React from "react";
import { GraphContextProvider } from "./GraphContext";
import { GraphCustomStyleReset } from "./GraphCustomStyleReset";
import { GraphHeader } from "./graphHeader/GraphHeader";
import { GraphModals } from "./graphModals/GraphModals";
import { GraphNetwork } from "./graphNetwork/GraphNetwork";

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
