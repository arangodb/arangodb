import React, { useEffect } from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { useUsername } from "./useFetchDatabasePermissions";
import { UserPermissionsTable } from "./UserPermissionsTable";

const useSetupUserPermissionsNav = () => {
  const { username } = useUsername();

  useEffect(() => {
    const breadcrumbDiv = document.querySelector(
      "#subNavigationBar .breadcrumb"
    );
    breadcrumbDiv?.append(`User: ${username}`);
    window.arangoHelper.buildUserSubNav(username, "Permissions");
  }, [username]);
};

export const UserPermissionsView = () => {
  useGlobalStyleReset();
  useSetupUserPermissionsNav();
  return (
    <ChakraCustomProvider>
      <UserPermissionsTable />
    </ChakraCustomProvider>
  );
};
