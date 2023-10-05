import { ChevronDownIcon, ChevronUpIcon } from "@chakra-ui/icons";
import { IconButton, Stack } from "@chakra-ui/react";
import { useQuery } from "@tanstack/react-query";
import {
  CellContext,
  createColumnHelper,
  getExpandedRowModel
} from "@tanstack/react-table";
import React from "react";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { getRouteForCurrentDB } from "../../../utils/arangoClient";

type PermissionType = "rw" | "ro" | "none" | "undefined";
type FullDatabasePermissionsType = {
  [databaseName: string]: {
    permission: PermissionType;
    collections?: {
      [collectionName: string]: PermissionType;
    };
  };
};

type DatabaseTableType = {
  databaseName: string;
  permission: PermissionType;
  collections: {
    collectionName: string;
    permission: PermissionType;
  }[];
};
const useFetchDatabasePermissions = () => {
  const { data } = useQuery(["permissions"], async () => {
    const username = window.location.hash.split("#user/")[1].split("/")[0];
    const url = `/_api/user/${encodeURIComponent(username)}/database`;
    const route = getRouteForCurrentDB(url);
    const data = await route.get({ full: "true" });
    return data.body.result as FullDatabasePermissionsType;
  });
  return { databasePermissions: data };
};

const usePermissionTableData = () => {
  const { databasePermissions } = useFetchDatabasePermissions();
  if (!databasePermissions)
    return {
      databaseTable: []
    };
  console.log({ databasePermissions });
  const databaseTable = Object.entries(databasePermissions).map(
    ([databaseName, permissionObject]) => {
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
    }
  );
  return {
    databaseTable: [...databaseTable]
  };
};
export const UserPermissionsView = () => {
  return (
    <ChakraCustomProvider>
      <UserPermissionsTable />
    </ChakraCustomProvider>
  );
};

const columnHelper = createColumnHelper<DatabaseTableType>();

const TABLE_COLUMNS = [
  {
    id: "expander",
    header: () => null,
    cell: (info: CellContext<DatabaseTableType, unknown>) => {
      return info.row.getCanExpand() ? (
        <IconButton
          variant="ghost"
          size="sm"
          aria-label="Expand/collapse row"
          icon={
            info.row.getIsExpanded() ? <ChevronDownIcon /> : <ChevronUpIcon />
          }
          onClick={info.row.getToggleExpandedHandler()}
        />
      ) : (
        ""
      );
    }
  },
  columnHelper.accessor("databaseName", {
    header: "Name",
    id: "databaseName",
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("permission", {
    header: "Permission",
    id: "permission",
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(row => (row.permission === "rw" ? true : false), {
    header: "Administrate",
    id: "rw",
    cell: info => {
      return info.cell.getValue() ? "Yes" : "No";
    },
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(row => (row.permission === "ro" ? true : false), {
    header: "Access",
    id: "ro",
    cell: info => {
      return info.cell.getValue() ? "Yes" : "No";
    },
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(row => (row.permission === "none" ? true : false), {
    header: "No Accesss",
    id: "none",
    cell: info => {
      return info.cell.getValue() ? "Yes" : "No";
    },
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(
    row => (row.permission === "undefined" ? true : false),
    {
      header: "Use Default",
      id: "undefined",
      cell: info => {
        return info.cell.getValue() ? "Yes" : "No";
      },
      meta: {
        filterType: "single-select"
      }
    }
  )
];

const UserPermissionsTable = () => {
  const { databaseTable } = usePermissionTableData();
  const tableInstance = useSortableReactTable<DatabaseTableType>({
    data: databaseTable || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "databaseName",
        desc: false
      }
    ],
    getExpandedRowModel: getExpandedRowModel(),
    getRowCanExpand: () => true
  });
  return (
    <Stack padding="4">
      <FiltersList<DatabaseTableType>
        columns={TABLE_COLUMNS}
        table={tableInstance}
      />
      <ReactTable<DatabaseTableType>
        table={tableInstance}
        emptyStateMessage="No database permissions found"
        renderSubComponent={row => {
          return (
            <td colSpan={6}>
              <Stack padding="4" width="full">
                <pre>{JSON.stringify(row.original.collections, null, 2)}</pre>
              </Stack>
            </td>
          );
        }}
      />
    </Stack>
  );
};
