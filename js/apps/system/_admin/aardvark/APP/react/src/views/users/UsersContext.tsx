import { CreateDatabaseUser } from "arangojs/database";
import React, { createContext, ReactNode, useContext } from "react";
//import { useFetchGraphs } from "./useFetchGraphs";

type UsersContextType = {
  users: CreateDatabaseUser[] | undefined;
};
const UsersContext = createContext<UsersContextType>({} as UsersContextType);

export const UsersProvider = ({ children }: { children: ReactNode }) => {
  //const { graphs } = useFetchGraphs();
  const users = [
    {
      username: "andreas",
      passwd: "password",
      active: true,
      extra: undefined
    },
    {
      username: "palash",
      passwd: "password",
      active: true,
      extra: undefined
    }
  ];
  return (
    <UsersContext.Provider
      value={{
        users
      }}
    >
      {children}
    </UsersContext.Provider>
  );
};

export const useUsersContext = () => {
  return useContext(UsersContext);
};
