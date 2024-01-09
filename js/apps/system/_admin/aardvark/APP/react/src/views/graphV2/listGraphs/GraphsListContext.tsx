import { GraphInfo } from "arangojs/graph";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchGraphs } from "./useFetchGraphs";
import { useIsOneShardDb } from "./useIsOneShardDb";
type GraphsListContextType = {
  graphs: GraphInfo[] | undefined;
  isOneShardDb: boolean;
};
const GraphsListContext = createContext<GraphsListContextType>(
  {} as GraphsListContextType
);

export const GraphsListProvider = ({ children }: { children: ReactNode }) => {
  const { graphs } = useFetchGraphs();
  const isOneShardDb = useIsOneShardDb();
  return (
    <GraphsListContext.Provider
      value={{
        graphs,
        isOneShardDb
      }}
    >
      {children}
    </GraphsListContext.Provider>
  );
};

export const useGraphsListContext = () => {
  return useContext(GraphsListContext);
};
