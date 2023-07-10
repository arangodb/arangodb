import React, { createContext, ReactNode, useContext } from "react";
import { CreateDatabaseUser } from "arangojs/database";

type UsersModeContextType = {
  initialUser?: CreateDatabaseUser;
  mode: "add" | "edit";
};
const UsersModeContext = createContext<UsersModeContextType>(
  {} as UsersModeContextType
);

export const UsersModeProvider = ({
  initialUser,
  children,
  mode
}: {
  initialUser?: CreateDatabaseUser;
  children: ReactNode;
  mode: "add" | "edit";
}) => {
  return (
    <UsersModeContext.Provider
      value={{
        initialUser,
        mode
      }}
    >
      {children}
    </UsersModeContext.Provider>
  );
};

export const useUsersModeContext = () => {
  return useContext(UsersModeContext);
};
