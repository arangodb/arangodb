import { createColumnHelper, Row } from "@tanstack/react-table";
import React from "react";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { PermissionSwitch } from "./PermissionSwitch";
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
      header: "Administrate",
      id: "rw",
      cell: info => {
        return <PermissionSwitch info={info} checked={info.cell.getValue()} />;
      },
      meta: {
        filterType: "single-select"
      }
    }
  ),
  collectionColumnHelper.accessor(
    row => (row.permission === "ro" ? true : false),
    {
      header: "Access",
      id: "ro",
      cell: info => {
        return <PermissionSwitch info={info} checked={info.cell.getValue()} />;
      },
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
        return <PermissionSwitch info={info} checked={info.cell.getValue()} />;
      },
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
        return <PermissionSwitch info={info} checked={info.cell.getValue()} />;
      },
      meta: {
        filterType: "single-select"
      }
    }
  )
];
const COLLECTION_COLUMNS = [
  {
    id: "expander",
    size: 8,
    header: () => null,
    cell: () => {
      return <></>;
    }
  },
  collectionColumnHelper.accessor("collectionName", {
    header: "Name",
    id: "collectionName",
    meta: {
      filterType: "text"
    },
    size: 100,
    maxSize: 200
  }),
  ...collectionPermissionColumns
];
export const CollectionsTable = ({ row }: { row: Row<DatabaseTableType> }) => {
  const tableInstance = useSortableReactTable<CollectionType>({
    data: row.original.collections || [],
    columns: COLLECTION_COLUMNS
  });
  return (
    <td colSpan={6}>
      <ReactTable<CollectionType> layout="fixed" table={tableInstance} />
    </td>
  );
};
