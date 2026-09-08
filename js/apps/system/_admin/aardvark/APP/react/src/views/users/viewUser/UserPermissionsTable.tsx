import { ReactTable, TableControl } from "@arangodb/ui";
import { Alert, AlertDescription, AlertIcon, Stack } from "@chakra-ui/react";
import React, { useEffect } from "react";
import {
  CollectionsPermissionsTable,
  DatabaseTableType
} from "./CollectionsPermissionsTable";
import { SystemDatabaseWarningModal } from "./SystemDatabaseWarningModal";
import {
  useFetchDatabasePermissions,
  useUsername
} from "./useFetchDatabasePermissions";
import { isRbacRejection } from "../../../utils/arangoClient";
import {
  UserPermissionsContextProvider,
  useUserPermissionsContext
} from "./UserPermissionsContext";

export const UserPermissionsTable = () => {
  return (
    <UserPermissionsContextProvider>
      <UserPermissionsTableInner />
    </UserPermissionsContextProvider>
  );
};

const UserPermissionsTableInner = () => {
  const { tableInstance } = useUserPermissionsContext();
  const { username } = useUsername();
  useEffect(() => {
    window.arangoHelper.buildUserSubNav(username, "Permissions");
  }, [username]);

  const { isManagedUser, isRootUser } = tableInstance.options.meta as any;
  const { error } = useFetchDatabasePermissions();

  if (isRbacRejection(error)) {
    return (
      <Stack padding="4">
        <Alert status="info">
          <AlertIcon />
          <AlertDescription>
            Classic database permissions are not available in RBAC mode. Access
            is granted through roles instead.
          </AlertDescription>
        </Alert>
      </Stack>
    );
  }

  return (
    <Stack padding="4">
      <SystemDatabaseWarningModal />
      <TableControl<DatabaseTableType>
        table={tableInstance}
        showColumnSelector={false}
      />
      {isManagedUser ? (
        <Alert status="error">
          <AlertIcon />
          <AlertDescription>
            This user's permissions are managed by the Arango Managed Platform
            (AMP) and cannot be modified in this deployment.
          </AlertDescription>
        </Alert>
      ) : null}
      <ReactTable<DatabaseTableType>
        tableWidth="auto"
        table={tableInstance}
        layout="fixed"
        emptyStateMessage="No database permissions found"
        getCellProps={cell => {
          if (cell.column.id === "databaseName") {
            return {
              padding: "0",
              height: "1px" // hack to make div take full height
            };
          }
        }}
        renderSubComponent={row => {
          return (
            <CollectionsPermissionsTable
              row={row}
              isManagedUser={isManagedUser}
              isRootUser={isRootUser}
            />
          );
        }}
      />
    </Stack>
  );
};
