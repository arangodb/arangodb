import { GraphInfo } from "arangojs/graph";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchGraphs } from "./useFetchGraphs";

type GraphsContextType = {
  graphs: GraphInfo[] | undefined;
  isFormDisabled: boolean;
};
const GraphsContext = createContext<GraphsContextType>({} as GraphsContextType);

export const GraphsProvider = ({
  children,
  isFormDisabled
}: {
  children: ReactNode;
  isFormDisabled?: boolean;
}) => {
  const { graphs } = useFetchGraphs();
  return (
    <GraphsContext.Provider
      value={{
        graphs,
        isFormDisabled: !!isFormDisabled
      }}
    >
      {children}
    </GraphsContext.Provider>
  );
};

export const useGraphsContext = () => {
  return useContext(GraphsContext);
};
