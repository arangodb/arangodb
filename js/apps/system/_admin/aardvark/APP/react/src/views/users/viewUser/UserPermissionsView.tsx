import React from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { UserPermissionsTable } from "./UserPermissionsTable";

export const UserPermissionsView = () => {
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider>
      <UserPermissionsTable />
    </ChakraCustomProvider>
  );
};
