import { Link, Stack } from "@chakra-ui/react";
import { CellContext, createColumnHelper } from "@tanstack/react-table";
import { Index } from "arangojs/indexes";
import React from "react";
import { FiltersList } from "../../../../components/table/FiltersList";
import { ReactTable } from "../../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../../components/table/useSortableReactTable";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { TYPE_TO_LABEL_MAP } from "../CollectionIndicesHelpers";
import { useSyncIndexCreationJob } from "../useSyncIndexCreationJob";
import { CollectionIndexActionButtons } from "./CollectionIndexActionButtons";

const columnHelper = createColumnHelper<Index>();

const IdCell = ({ info }: { info: CellContext<Index, string> }) => {
  const cellValue = info.cell.getValue();
  const id = cellValue.slice(cellValue.lastIndexOf("/") + 1);
  const collectionName = window.location.hash.split("#cIndices/")[1];
  // need to use href here instead of RouteLink due to a bug in react-router
  return (
    <Link
      href={`#cIndices/${collectionName}/${id}`}
      textDecoration="underline"
      color="blue.500"
      _hover={{
        color: "blue.600"
      }}
    >
      {id}
    </Link>
  );
};
const TABLE_COLUMNS = [
  columnHelper.accessor("id", {
    header: "ID",
    id: "id",
    cell: info => <IdCell info={info} />,
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
      return (
        <CollectionIndexActionButtons collectionIndex={info.row.original} />
      );
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
