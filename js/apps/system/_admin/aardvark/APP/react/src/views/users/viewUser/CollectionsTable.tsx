import { Tag } from "@chakra-ui/react";
import { CellContext, createColumnHelper, Row } from "@tanstack/react-table";
import React from "react";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { getCurrentDB } from "../../../utils/arangoClient";
import { CollectionPermissionSwitch } from "./CollectionPermissionSwitch";
import { getIsDefaultRow } from "./DatabasePermissionSwitch";
import { PermissionType, useUsername } from "./useFetchDatabasePermissions";

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
        return (
          <CollectionPermissionSwitch
            info={info}
            checked={info.cell.getValue()}
          />
        );
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
        return (
          <CollectionPermissionSwitch
            info={info}
            checked={info.cell.getValue()}
          />
        );
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
        return (
          <CollectionPermissionSwitch
            info={info}
            checked={info.cell.getValue()}
          />
        );
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
        return (
          <CollectionPermissionSwitch
            info={info}
            checked={info.cell.getValue()}
          />
        );
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
    header: "Collections",
    id: "collectionName",
    cell: info => {
      if (getIsDefaultRow(info)) {
        return <Tag>Default</Tag>;
      }
      return info.cell.getValue();
    },
    meta: {
      filterType: "text"
    },
    size: 150,
    maxSize: 200
  }),
  ...collectionPermissionColumns
];
export const CollectionsTable = ({
  row,
  databaseTable,
  refetchDatabasePermissions
}: {
  row: Row<DatabaseTableType>;
  databaseTable: DatabaseTableType[];
  refetchDatabasePermissions: (() => void) | undefined;
}) => {
  const { databaseName } = row.original;
  const { username } = useUsername();
  const handleCellClick = async ({
    permission,
    info
  }: {
    permission: string;
    info: CellContext<CollectionType, unknown>;
  }) => {
    const { collectionName } = info.row.original;
    const currentDbRoute = getCurrentDB().route(
      `_api/user/${username}/database/${databaseName}/${collectionName}`
    );
    if (permission === "undefined") {
      await currentDbRoute.delete();
      refetchDatabasePermissions?.();
      return;
    }
    await currentDbRoute.put({ grant: permission });
    refetchDatabasePermissions?.();
  };
  const tableInstance = useSortableReactTable<CollectionType>({
    data: row.original.collections || [],
    columns: COLLECTION_COLUMNS,
    initialSorting: [
      {
        id: "collectionName",
        desc: false
      }
    ],
    meta: {
      databaseTable,
      databaseName,
      handleCellClick
    }
  });

  return (
    <td colSpan={6}>
      <ReactTable<CollectionType> layout="fixed" table={tableInstance} />
    </td>
  );
};
