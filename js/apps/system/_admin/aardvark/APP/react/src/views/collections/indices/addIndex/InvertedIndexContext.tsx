import React, { createContext, ReactNode, useContext, useState } from "react";

export type FieldData = {
  fieldName: string;
  fieldValue: string;
  fieldIndex: number;
};
type InvertedIndexContextType = {
  currentFieldData?: FieldData;
  setCurrentFieldData: (data?: FieldData) => void;
};
const InvertedIndexContext = createContext<InvertedIndexContextType>(
  {} as InvertedIndexContextType
);

export const InvertedIndexProvider = ({
  children
}: {
  children: ReactNode;
}) => {
  const [currentFieldData, setCurrentFieldData] = useState<FieldData>();
  return (
    <InvertedIndexContext.Provider
      value={{
        setCurrentFieldData,
        currentFieldData
      }}
    >
      {children}
    </InvertedIndexContext.Provider>
  );
};

export const useInvertedIndexContext = () => {
  return useContext(InvertedIndexContext);
};
