import { GraphInfo } from "arangojs/graph";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchGraphs } from "./useFetchGraphs";

type GraphsContextType = {
  graphs: GraphInfo[] | undefined;
};
const GraphsContext = createContext<GraphsContextType>({} as GraphsContextType);

export const GraphsProvider = ({ children }: { children: ReactNode }) => {
  const { graphs } = useFetchGraphs();
  return (
    <GraphsContext.Provider
      value={{
        graphs
      }}
    >
      {children}
    </GraphsContext.Provider>
  );
};

export const useGraphsContext = () => {
  return useContext(GraphsContext);
};
