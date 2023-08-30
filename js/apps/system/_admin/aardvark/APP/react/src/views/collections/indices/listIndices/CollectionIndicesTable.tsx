import { Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import { Index } from "arangojs/indexes";
import React from "react";
import { FiltersList } from "../../../../components/table/FiltersList";
import { ReactTable } from "../../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../../components/table/useSortableReactTable";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { TYPE_TO_LABEL_MAP } from "../CollectionIndicesHelpers";
import { CollectionIndexActionButtons } from "./CollectionIndexActionButtons";
import { useSyncIndexCreationJob } from "../useSyncIndexCreationJob";

const columnHelper = createColumnHelper<Index>();

const TABLE_COLUMNS = [
  columnHelper.accessor("id", {
    header: "ID",
    id: "id",
    cell: info => {
      const cellValue = info.cell.getValue();
      return cellValue.slice(cellValue.lastIndexOf("/") + 1);
    },
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor(row => TYPE_TO_LABEL_MAP[row.type], {
    header: "Type",
    id: "type",
    filterFn: "arrIncludesSome",
    meta: {
      filterType: "multi-select"
    }
  }),
  columnHelper.display({
    header: "Actions",
    id: "actions",
    cell: info => {
      return <CollectionIndexActionButtons index={info.row.original} />;
    }
  })
];

export const CollectionIndicesTable = () => {
  useSyncIndexCreationJob();
  const { collectionIndices } = useCollectionIndicesContext();
  const tableInstance = useSortableReactTable<Index>({
    data: collectionIndices || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "id",
        desc: false
      }
    ],
    initialFilters: []
  });
  return (
    <Stack>
      <FiltersList<Index> columns={TABLE_COLUMNS} table={tableInstance} />
      <ReactTable<Index>
        table={tableInstance}
        emptyStateMessage="No indexes found"
      />
    </Stack>
  );
};
