import React, { createContext, ReactNode, useContext, useState } from "react";

type LinksContextType = {
  currentField: CurrentFieldData | undefined;
  setCurrentField: (field: CurrentFieldData | undefined) => void;
};
const LinksContext = createContext<LinksContextType>({} as LinksContextType);

export const useLinksContext = () => useContext(LinksContext);

type CurrentFieldData = {
  fieldPath: string;
};

export const LinksContextProvider = ({ children }: { children: ReactNode }) => {
  const [currentField, setCurrentField] = useState<
    CurrentFieldData | undefined
  >();
  return (
    <LinksContext.Provider
      value={{
        currentField,
        setCurrentField
      }}
    >
      {children}
    </LinksContext.Provider>
  );
};
