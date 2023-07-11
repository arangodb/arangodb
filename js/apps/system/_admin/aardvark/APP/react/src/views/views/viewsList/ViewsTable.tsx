import { Box, Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { Link as RouterLink, useHistory } from "react-router-dom";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { LockableViewDescription, useFetchViews } from "../useFetchViews";

const columnHelper = createColumnHelper<LockableViewDescription>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          as={RouterLink}
          to={`/views/${encodeURIComponent(cellValue)}`}
          textDecoration="underline"
          color="blue.500"
          _hover={{
            color: "blue.600"
          }}
        >
          {cellValue}
          {info.row.getValue("isLocked") && (
            <Box
              as="i"
              fontSize={"sm"}
              className={"fa fa-spinner fa-spin"}
              marginLeft={1}
            />
          )}
        </Link>
      );
    },
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("type", {
    header: "Type",
    id: "type",
    filterFn: "arrIncludesSome",
    meta: {
      filterType: "multi-select"
    }
  })
];

export const ViewsTable = () => {
  const { views } = useFetchViews();
  const tableInstance = useSortableReactTable<LockableViewDescription>({
    data: views || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    initialFilters: []
  });
  const history = useHistory();
  return (
    <Stack>
      <FiltersList<LockableViewDescription>
        columns={TABLE_COLUMNS}
        table={tableInstance}
      />
      <ReactTable<LockableViewDescription>
        table={tableInstance}
        emptyStateMessage="No views found"
        onRowSelect={row => {
          history.push(`/views/${row.original.name}`);
        }}
      />
    </Stack>
  );
};
