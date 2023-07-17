import { GraphInfo } from "arangojs/graph";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchGraphs } from "./useFetchGraphs";

type GraphsListContextType = {
  graphs: GraphInfo[] | undefined;
};
const GraphsListContext = createContext<GraphsListContextType>({} as GraphsListContextType);

export const GraphsListProvider = ({ children }: { children: ReactNode }) => {
  const { graphs } = useFetchGraphs();
  return (
    <GraphsListContext.Provider
      value={{
        graphs
      }}
    >
      {children}
    </GraphsListContext.Provider>
  );
};

export const useGraphsListContext = () => {
  return useContext(GraphsListContext);
};
