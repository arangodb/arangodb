import React, { createContext, ReactNode, useContext } from "react";
import { useFetchDatabases } from "./useFetchDatabases";
import { DatabaseDescription } from "./Database.types";

type DatabasesContextType = {
  databases: DatabaseDescription[] | undefined;
  isFormDisabled: boolean;
};
const DatabasesContext = createContext<DatabasesContextType>(
  {} as DatabasesContextType
);

export const DatabasesProvider = ({
  children,
  isFormDisabled
}: {
  children: ReactNode;
  isFormDisabled?: boolean;
}) => {
  const { databases } = useFetchDatabases();
  return (
    <DatabasesContext.Provider
      value={{
        databases,
        isFormDisabled: !!isFormDisabled
      }}
    >
      {children}
    </DatabasesContext.Provider>
  );
};

export const useDatabasesContext = () => {
  return useContext(DatabasesContext);
};
