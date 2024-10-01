import { GraphInfo } from "arangojs/graph";
import React, { createContext, ReactNode, useContext } from "react";

type GraphsModeContextType = {
  initialGraph?: GraphInfo;
  mode: "add" | "edit";
};
const GraphsModeContext = createContext<GraphsModeContextType>(
  {} as GraphsModeContextType
);

export const GraphsModeProvider = ({
  initialGraph,
  children,
  mode
}: {
  initialGraph?: GraphInfo;
  children: ReactNode;
  mode: "add" | "edit";
}) => {
  return (
    <GraphsModeContext.Provider
      value={{
        initialGraph,
        mode
      }}
    >
      {children}
    </GraphsModeContext.Provider>
  );
};

export const useGraphsModeContext = () => {
  return useContext(GraphsModeContext);
};
