import { AnalyzerDescription } from "arangojs/analyzer";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchAnalyzers } from "./useFetchAnalyzers";

type AnalyzersContextType = {
  analyzers: AnalyzerDescription[] | undefined;
};
const AnalyzersContext = createContext<AnalyzersContextType>(
  {} as AnalyzersContextType
);

export const AnalyzersProvider = ({ children }: { children: ReactNode }) => {
  const { analyzers } = useFetchAnalyzers();
  return (
    <AnalyzersContext.Provider
      value={{
        analyzers,
   
      }}
    >
      {children}
    </AnalyzersContext.Provider>
  );
};

export const useAnalyzersContext = () => {
  return useContext(AnalyzersContext);
};
