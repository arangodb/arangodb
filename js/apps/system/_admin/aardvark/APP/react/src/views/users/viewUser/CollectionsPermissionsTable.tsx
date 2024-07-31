import { Box, Flex, Tag } from "@chakra-ui/react";
import { createColumnHelper, Row } from "@tanstack/react-table";
import React from "react";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { CollectionDefaultRowWarningPopover } from "./CollectionDefaultRowWarningPopover";
import { CollectionPermissionSwitch } from "./CollectionPermissionSwitch";
import { getIsDefaultRow } from "./DatabasePermissionSwitch";
import { PermissionType } from "./useFetchDatabasePermissions";

export type CollectionType = {
  collectionName: string;
  permission: PermissionType;
};
export type DatabaseTableType = {
  databaseName: string;
  permission: PermissionType;
  collections: CollectionType[];
};
const collectionColumnHelper = createColumnHelper<CollectionType>();
const collectionPermissionColumns = [
  collectionColumnHelper.accessor(
    row => (row.permission === "rw" ? true : false),
    {
      header: "Read/Write",
      id: "rw",
      cell: info => {
        return (
          <CollectionPermissionSwitch
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
  ),
  collectionColumnHelper.accessor(
    row => (row.permission === "ro" ? true : false),
    {
      header: "Read only",
      id: "ro",
      cell: info => {
        return (
          <CollectionPermissionSwitch
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
  ),
  collectionColumnHelper.accessor(
    row => (row.permission === "none" ? true : false),
    {
      header: "No Accesss",
      id: "none",
      cell: info => {
        return (
          <CollectionPermissionSwitch
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
  ),
  collectionColumnHelper.accessor(
    row => (row.permission === "undefined" ? true : false),
    {
      header: "Use Default",
      id: "undefined",
      cell: info => {
        return (
          <CollectionPermissionSwitch
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
const COLLECTION_COLUMNS = [
  collectionColumnHelper.accessor("collectionName", {
    header: "Collections",
    id: "collectionName",
    cell: info => {
      const { databaseName } = (info.table.options.meta || {}) as any;
      if (getIsDefaultRow(info)) {
        return (
          <Flex alignItems="center">
            <Tag>Default</Tag>
            <CollectionDefaultRowWarningPopover databaseName={databaseName} />
          </Flex>
        );
      }
      return info.cell.getValue();
    },
    sortingFn: (rowA, rowB, columnId) => {
      if (rowA.original.collectionName === "*") {
        return 0;
      }
      return rowA
        .getValue<string>(columnId)
        ?.localeCompare(rowB.getValue<string>(columnId) || "");
    },
    meta: {
      filterType: "text"
    },
    size: 150,
    maxSize: 150
  }),
  ...collectionPermissionColumns
];
export const CollectionsPermissionsTable = ({
  row,
  isManagedUser,
  isRootUser
}: {
  row: Row<DatabaseTableType>;
  isManagedUser: boolean;
  isRootUser: boolean;
}) => {
  const { databaseName } = row.original;
  const tableInstance = useSortableReactTable<CollectionType>({
    data: row.original.collections || [],
    columns: COLLECTION_COLUMNS,
    defaultSorting: [
      {
        id: "collectionName",
        desc: false
      }
    ],
    defaultFilters: [],
    storageKey: "collection-permissions-table",
    meta: {
      databaseName,
      isManagedUser,
      isRootUser
    }
  });

  return (
    <Box as="td" colSpan={6} paddingY="4">
      <ReactTable<CollectionType>
        tableWidth="auto"
        layout="fixed"
        backgroundColor="gray.50"
        table={tableInstance}
      />
    </Box>
  );
};
