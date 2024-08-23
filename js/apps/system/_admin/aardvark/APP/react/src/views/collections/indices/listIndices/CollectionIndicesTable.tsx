import { Link, Spinner, Stack, Text } from "@chakra-ui/react";
import { CellContext, createColumnHelper } from "@tanstack/react-table";
import { Index } from "arangojs/indexes";
import React from "react";
import { ReactTable } from "../../../../components/table/ReactTable";
import { TableControl } from "../../../../components/table/TableControl";
import { useSortableReactTable } from "../../../../components/table/useSortableReactTable";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { TYPE_TO_LABEL_MAP } from "../CollectionIndicesHelpers";
import { useSyncIndexCreationJob } from "../useSyncIndexCreationJob";
import { CollectionIndexActionButtons } from "./CollectionIndexActionButtons";

const columnHelper = createColumnHelper<Index & { progress?: number }>();

const NameCell = ({
  info
}: {
  info: CellContext<Index & { progress?: number }, string>;
}) => {
  const id = info.row.original.id;
  const finalId = id.slice(id.lastIndexOf("/") + 1); // remove the collection name from the id
  const collectionName = window.location.hash.split("#cIndices/")[1]; // get the collection name from the url
  // need to use href here instead of RouteLink due to a bug in react-router
  return typeof info.row.original.progress === "number" &&
    info.row.original.progress < 100 ? (
    <Text>
      {info.cell.getValue()} <Spinner size="xs" />{" "}
      {info.row.original.progress.toFixed(0)}%
    </Text>
  ) : (
    <Link
      href={`#cIndices/${collectionName}/${finalId}`}
      textDecoration="underline"
      color="blue.500"
      _hover={{
        color: "blue.600"
      }}
    >
      {info.cell.getValue()}
    </Link>
  );
};
const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => <NameCell info={info} />,
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
  columnHelper.accessor(
    row => {
      return row.fields
        .map(field => {
          if (typeof field === "string") return field;
          return field.name;
        })
        .join(", ");
    },
    {
      header: "Fields",
      id: "fields",
      meta: {
        filterType: "text"
      }
    }
  ),
  columnHelper.accessor("unique", {
    header: "Unique",
    id: "unique",
    cell: info => {
      return `${info.cell.getValue()}`;
    },
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(
    row => {
      if ((row as any).selectivityEstimate) {
        const selectivityEstimate = (row as any).selectivityEstimate;
        // percentage rounded to 2 decimal places
        return `${(selectivityEstimate * 100).toFixed(2)}%`;
      }
      return "N/A";
    },
    {
      header: "Selectivity Estimate",
      id: "selectivityEstimate",
      meta: {
        filterType: "text"
      }
    }
  ),
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
    defaultSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    defaultFilters: [],
    storageKey: "collection-indices"
  });
  return (
    <Stack>
      <TableControl<Index & { progress?: number }>
        table={tableInstance}
        columns={TABLE_COLUMNS}
      />
      <ReactTable<Index & { progress?: number }>
        table={tableInstance}
        emptyStateMessage="No indexes found"
        onRowSelect={row => {
          if (
            typeof row.original.progress === "number" &&
            row.original.progress < 100
          ) {
            return;
          }
          const finalId = row.original.id.slice(
            row.original.id.lastIndexOf("/") + 1
          );
          const collectionName = window.location.hash.split("#cIndices/")[1];
          window.location.hash = `#cIndices/${collectionName}/${finalId}`;
        }}
      />
    </Stack>
  );
};
