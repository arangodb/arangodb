import { Link, Spinner, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { useHistory } from "react-router-dom";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { createEncodedUrl } from "../../../utils/urlHelper";
import { LockableViewDescription, useFetchViews } from "../useFetchViews";

const columnHelper = createColumnHelper<LockableViewDescription>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      const href = createEncodedUrl({ path: "views", value: cellValue });
      return (
        <Link
          href={href}
          textDecoration="underline"
          color="blue.500"
          _hover={{
            color: "blue.600"
          }}
        >
          {cellValue}
          {info.row.getValue("isLocked") && (
            <Spinner size={"xs"} marginLeft={1} />
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
