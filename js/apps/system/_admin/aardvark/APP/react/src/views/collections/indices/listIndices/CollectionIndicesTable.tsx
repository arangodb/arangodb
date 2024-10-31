import { Link, Spinner, Stack, Text } from "@chakra-ui/react";
import { CellContext, createColumnHelper } from "@tanstack/react-table";
import { HiddenIndex } from "arangojs/indexes";
import { isArray, isObject } from "lodash";
import React from "react";
import { ReactTable } from "../../../../components/table/ReactTable";
import { TableControl } from "../../../../components/table/TableControl";
import { useSortableReactTable } from "../../../../components/table/useSortableReactTable";
import { useCollectionIndicesContext } from "../CollectionIndicesContext";
import { TYPE_TO_LABEL_MAP } from "../CollectionIndicesHelpers";
import { useSyncIndexCreationJob } from "../useSyncIndexCreationJob";
import { CollectionIndexActionButtons } from "./CollectionIndexActionButtons";

const columnHelper = createColumnHelper<HiddenIndex>();

const NameCell = ({ info }: { info: CellContext<HiddenIndex, unknown> }) => {
  const { id, progress } = info.row.original;
  const finalId = id.slice(id.lastIndexOf("/") + 1); // remove the collection name from the id
  const collectionName = window.location.hash.split("#cIndices/")[1]; // get the collection name from the url

  const name = info.cell.getValue();
  // name may not exist for HiddenIndex
  const finalName = typeof name === "string" ? name : finalId;

  if (typeof progress === "number" && progress < 100) {
    return (
      <Text>
        {finalName} <Spinner size="xs" /> {progress.toFixed(0)}%
      </Text>
    );
  }

  // need to use href here instead of RouteLink due to a bug in react-router
  return (
    <Link
      href={`#cIndices/${collectionName}/${finalId}`}
      textDecoration="underline"
      color="blue.500"
      _hover={{
        color: "blue.600"
      }}
    >
      {finalName}
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
      if (!row.fields) {
        return "-";
      }
      if (isArray(row.fields)) {
        if (row.fields.length === 0) {
          return "-";
        }
        return row.fields
          .map(field => {
            if (typeof field === "string") {
              return field;
            }
            return field.name;
          })
          .join(", ");
      }
      if (isObject(row.fields)) {
        if (Object.keys(row.fields).length === 0) {
          return "-";
        }
        return Object.keys(row.fields).join(", ");
      }
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
  const tableInstance = useSortableReactTable<HiddenIndex>({
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
      <TableControl<HiddenIndex>
        table={tableInstance}
        columns={TABLE_COLUMNS}
      />
      <ReactTable<HiddenIndex>
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
