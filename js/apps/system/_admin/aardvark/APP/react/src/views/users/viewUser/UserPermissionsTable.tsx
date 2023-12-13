import { Stack } from "@chakra-ui/react";
import React from "react";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import {
  CollectionsPermissionsTable,
  DatabaseTableType
} from "./CollectionsPermissionsTable";
import { SystemDatabaseWarningModal } from "./SystemDatabaseWarningModal";
import {
  TABLE_COLUMNS,
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

  return (
    <Stack padding="4">
      <SystemDatabaseWarningModal />
      <FiltersList<DatabaseTableType>
        columns={TABLE_COLUMNS}
        table={tableInstance}
      />
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
          return <CollectionsPermissionsTable row={row} />;
        }}
      />
    </Stack>
  );
};
