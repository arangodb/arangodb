import { AnalyzerDescription } from "arangojs/analyzer";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchAnalyzers } from "./useFetchAnalyzers";

type AnalyzersContextType = {
  analyzers: AnalyzerDescription[] | undefined;
  isFormDisabled: boolean;
};
const AnalyzersContext = createContext<AnalyzersContextType>(
  {} as AnalyzersContextType
);

export const AnalyzersProvider = ({
  children,
  isFormDisabled
}: {
  children: ReactNode;
  isFormDisabled?: boolean;
}) => {
  const { analyzers } = useFetchAnalyzers();
  return (
    <AnalyzersContext.Provider
      value={{
        analyzers,
        isFormDisabled: !!isFormDisabled
      }}
    >
      {children}
    </AnalyzersContext.Provider>
  );
};

export const useAnalyzersContext = () => {
  return useContext(AnalyzersContext);
};
