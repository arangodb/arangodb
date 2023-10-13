import React from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { DatabaseTableType } from "./CollectionsTable";
import { useFetchDatabasePermissions } from "./useFetchDatabasePermissions";
import { UserPermissionsTable } from "./UserPermissionsTable";

export const usePermissionTableData = () => {
  const { databasePermissions } = useFetchDatabasePermissions();
  if (!databasePermissions)
    return {
      databaseTable: []
    };
  const databaseTable = Object.entries(databasePermissions)
    .map(([databaseName, permissionObject]) => {
      return {
        databaseName,
        permission: permissionObject.permission,
        collections: Object.entries(permissionObject.collections || {}).map(
          ([collectionName, collectionPermission]) => {
            return {
              collectionName,
              permission: collectionPermission
            };
          }
        )
      };
    })
    .filter(Boolean) as DatabaseTableType[];
  return {
    databaseTable: [...databaseTable]
  };
};
export const UserPermissionsView = () => {
  useGlobalStyleReset();
  return (
    <ChakraCustomProvider>
      <UserPermissionsTable />
    </ChakraCustomProvider>
  );
};
