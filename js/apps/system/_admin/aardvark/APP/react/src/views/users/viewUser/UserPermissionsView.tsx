import React from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { DatabaseTableType } from "./CollectionsTable";
import { useFetchDatabasePermissions } from "./useFetchDatabasePermissions";
import { UserPermissionsTable } from "./UserPermissionsTable";

export const usePermissionTableData = () => {
  const { databasePermissions, refetchDatabasePermissions } =
    useFetchDatabasePermissions();
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
  let serverLevelDefaultPermission;
  try {
    serverLevelDefaultPermission = databasePermissions["*"].permission;
  } catch (ignore) {
    // just ignore, not part of the response
  }
  return {
    databaseTable: [...databaseTable],
    serverLevelDefaultPermission,
    refetchDatabasePermissions
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
