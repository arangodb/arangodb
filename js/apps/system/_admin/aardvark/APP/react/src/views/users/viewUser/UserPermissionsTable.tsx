import { ChevronDownIcon, ChevronUpIcon } from "@chakra-ui/icons";
import { IconButton, Stack, Tag, Text } from "@chakra-ui/react";
import {
  CellContext,
  createColumnHelper,
  getExpandedRowModel
} from "@tanstack/react-table";
import React, { useEffect } from "react";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { getCurrentDB } from "../../../utils/arangoClient";
import { CollectionsTable, DatabaseTableType } from "./CollectionsTable";
import {
  DatabasePermissionSwitch,
  getIsDefaultRow
} from "./DatabasePermissionSwitch";
import {
  useFetchDatabasePermissions,
  useUsername
} from "./useFetchDatabasePermissions";

const usePermissionTableData = () => {
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

const columnHelper = createColumnHelper<DatabaseTableType>();

const permissionColumns = [
  columnHelper.accessor(row => (row.permission === "rw" ? true : false), {
    header: "Administrate",
    id: "rw",
    cell: info => {
      return (
        <DatabasePermissionSwitch info={info} checked={info.cell.getValue()} />
      );
    },
    enableSorting: false,
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(row => (row.permission === "ro" ? true : false), {
    header: "Access",
    id: "ro",
    cell: info => {
      return (
        <DatabasePermissionSwitch info={info} checked={info.cell.getValue()} />
      );
    },
    enableSorting: false,
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(row => (row.permission === "none" ? true : false), {
    header: "No Accesss",
    id: "none",
    cell: info => {
      return (
        <DatabasePermissionSwitch info={info} checked={info.cell.getValue()} />
      );
    },
    enableSorting: false,
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
        return (
          <DatabasePermissionSwitch
            info={info}
            checked={info.cell.getValue()}
          />
        );
      },
      enableSorting: false,
      meta: {
        filterType: "single-select"
      }
    }
  )
];
const TABLE_COLUMNS = [
  {
    id: "expander",
    header: () => null,
    size: 2,
    cell: (info: CellContext<DatabaseTableType, unknown>) => {
      if (getIsDefaultRow(info) || !info.row.getCanExpand()) {
        return "";
      }
      return (
        <IconButton
          display="block"
          width="100%"
          padding="0"
          minWidth="4"
          variant="unstyled"
          size="sm"
          aria-label="Expand/collapse row"
          icon={
            info.row.getIsExpanded() ? <ChevronUpIcon /> : <ChevronDownIcon />
          }
          onClick={info.row.getToggleExpandedHandler()}
        />
      );
    }
  },
  columnHelper.accessor("databaseName", {
    header: "Name",
    id: "databaseName",
    size: 150,
    maxSize: 200,
    meta: {
      filterType: "text"
    },
    cell: info => {
      if (getIsDefaultRow(info)) {
        return <Tag>Default</Tag>;
      }
      if (info.row.getIsExpanded()) {
        return <Text fontWeight="semibold">{info.cell.getValue()}</Text>;
      }
      return info.cell.getValue();
    }
  }),
  ...permissionColumns
];
export const UserPermissionsTable = () => {
  const { databaseTable, refetchDatabasePermissions } =
    usePermissionTableData();
  const { username } = useUsername();
  const handleCellClick = async ({
    info,
    permission
  }: {
    info: CellContext<DatabaseTableType, unknown>;
    permission: string;
  }) => {
    const { databaseName } = info.row.original;
    const currentDbRoute = getCurrentDB().route(
      `_api/user/${username}/database/${databaseName}`
    );
    if (permission === "undefined") {
      await currentDbRoute.delete();
      refetchDatabasePermissions?.();
      return;
    }
    await currentDbRoute.put({
      grant: permission
    });
    refetchDatabasePermissions?.();
  };
  const tableInstance = useSortableReactTable<DatabaseTableType>({
    data: databaseTable || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "databaseName",
        desc: false
      }
    ],
    meta: {
      handleCellClick
    },
    getExpandedRowModel: getExpandedRowModel(),
    getRowCanExpand: () => true
  });
  useEffect(() => {
    window.arangoHelper.buildUserSubNav(username, "Permissions");
  });

  return (
    <Stack padding="4">
      <FiltersList<DatabaseTableType>
        columns={TABLE_COLUMNS}
        table={tableInstance}
      />
      <ReactTable<DatabaseTableType>
        layout="fixed"
        table={tableInstance}
        emptyStateMessage="No database permissions found"
        renderSubComponent={row => {
          return (
            <CollectionsTable
              refetchDatabasePermissions={refetchDatabasePermissions}
              databaseTable={databaseTable}
              row={row}
            />
          );
        }}
      />
    </Stack>
  );
};
