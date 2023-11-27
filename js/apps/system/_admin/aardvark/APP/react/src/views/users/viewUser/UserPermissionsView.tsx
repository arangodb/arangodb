import React from "react";
import { useSetupBreadcrumbs } from "../../../components/hooks/useSetupBreadcrumbs";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { useUsername } from "./useFetchDatabasePermissions";
import { UserPermissionsTable } from "./UserPermissionsTable";

const useSetupUserPermissionsNav = () => {
  const { username } = useUsername();
  const breadcrumbText = `User: ${decodeURIComponent(username)}`;
  useSetupBreadcrumbs({ breadcrumbText });
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
