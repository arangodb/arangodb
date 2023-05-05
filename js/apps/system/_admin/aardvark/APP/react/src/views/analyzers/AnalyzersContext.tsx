import { AnalyzerDescription } from "arangojs/analyzer";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchAnalyzers } from "./useFetchAnalyzers";

type AnalyzersContextType = {
  analyzers: AnalyzerDescription[] | undefined;
  showSystemAnalyzers: boolean;
  setShowSystemAnalyzers: (value: boolean) => void;
  initialValues: { [key: string]: any };
  setInitialValues: (value: { [key: string]: any }) => void;
  viewAnalyzerName: string;
  setViewAnalyzerName: (value: string) => void;
  isFormDisabled: boolean;
};
const AnalyzersContext = createContext<AnalyzersContextType>(
  {} as AnalyzersContextType
);

export const AnalyzersProvider = ({ children }: { children: ReactNode }) => {
  const [viewAnalyzerName, setViewAnalyzerName] = React.useState<string>("");
  const [showSystemAnalyzers, setShowSystemAnalyzers] = React.useState(false);
  const [initialValues, setInitialValues] = React.useState<{
    [key: string]: any;
  }>({
    name: "",
    type: "identity",
    features: []
  });
  const { analyzers } = useFetchAnalyzers();
  const isFormDisabled = !!viewAnalyzerName;
  return (
    <AnalyzersContext.Provider
      value={{
        analyzers,
        showSystemAnalyzers,
        setShowSystemAnalyzers,
        initialValues,
        setInitialValues,
        viewAnalyzerName,
        setViewAnalyzerName,
        isFormDisabled
      }}
    >
      {children}
    </AnalyzersContext.Provider>
  );
};

export const useAnalyzersContext = () => {
  return useContext(AnalyzersContext);
};
