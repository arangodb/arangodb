import React, { createContext, ReactNode, useContext } from "react";
import { DatabaseUserValues } from "./addUser/CreateUser.types";
import { useFetchUsers } from "./useFetchUsers";

type UsersContextType = {
  users: DatabaseUserValues[] | undefined;
};
const UsersContext = createContext<UsersContextType>({} as UsersContextType);

export const UsersProvider = ({ children }: { children: ReactNode }) => {
  const { users } = useFetchUsers();
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
