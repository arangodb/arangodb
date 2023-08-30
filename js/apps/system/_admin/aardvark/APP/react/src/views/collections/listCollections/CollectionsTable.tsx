import { Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { useCollectionsContext } from "../CollectionsContext";
import { STATUS_TO_LABEL_MAP, TYPE_TO_LABEL_MAP } from "../CollectionsHelpers";
import { LockableCollectionDescription } from "../useFetchCollections";

const columnHelper = createColumnHelper<LockableCollectionDescription>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          href={`#cInfo/${encodeURIComponent(cellValue)}`}
          textDecoration="underline"
          color="blue.500"
          _hover={{
            color: "blue.600"
          }}
        >
          {cellValue}
        </Link>
      );
    },
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor(row => TYPE_TO_LABEL_MAP[row.type], {
    header: "Type",
    id: "type",
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(
    row => {
      return row.isSystem ? "System" : "Custom";
    },
    {
      header: "Source",
      id: "source",
      meta: {
        filterType: "single-select"
      }
    }
  ),
  columnHelper.accessor(
    row => (row.isLocked ? "Locked" : STATUS_TO_LABEL_MAP[row.status]),
    {
      header: "Status",
      id: "status",
      cell: info => {
        const cellValue = info.cell.getValue();
        if (info.row.original.isLocked && info.row.original.lockReason) {
          return info.row.original.lockReason;
        }
        return cellValue;
      },
      filterFn: "arrIncludesSome",
      meta: {
        filterType: "multi-select"
      }
    }
  )
];

export const CollectionsTable = () => {
  const { collections } = useCollectionsContext();
  const tableInstance = useSortableReactTable<LockableCollectionDescription>({
    data: collections || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    initialFilters: [
      {
        id: "source",
        value: "Custom"
      }
    ]
  });
  return (
    <Stack>
      <FiltersList<LockableCollectionDescription>
        columns={TABLE_COLUMNS}
        table={tableInstance}
      />
      <ReactTable<LockableCollectionDescription>
        table={tableInstance}
        emptyStateMessage="No collections found"
      />
    </Stack>
  );
};
