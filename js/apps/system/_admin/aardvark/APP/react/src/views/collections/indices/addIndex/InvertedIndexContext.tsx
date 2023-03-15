import React, { createContext, ReactNode, useContext, useState } from "react";

type InvertedIndexContextType = {
  currentFieldPath?: string;
  setCurrentFieldPath: (path: string) => void;
};
const InvertedIndexContext = createContext<InvertedIndexContextType>(
  {} as InvertedIndexContextType
);

export const InvertedIndexProvider = ({
  children
}: {
  children: ReactNode;
}) => {
  const [currentFieldPath, setCurrentFieldPath] = useState("");
  return (
    <InvertedIndexContext.Provider
      value={{
        setCurrentFieldPath,
        currentFieldPath
      }}
    >
      {children}
    </InvertedIndexContext.Provider>
  );
};

export const useInvertedIndexContext = () => {
  return useContext(InvertedIndexContext);
};
