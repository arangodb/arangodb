import { Link, Spinner, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { useHistory } from "react-router-dom";
import { ReactTable } from "../../../components/table/ReactTable";
import { TableControl } from "../../../components/table/TableControl";
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
      const href = createEncodedUrl({ path: "view", value: cellValue });
      return (
        <Link
          href={href}
          textDecoration="underline"
          color="blue.500"
          _hover={{
            color: "blue.600"
          }}
        >
          <>
            {cellValue}
            {info.row.getValue("isLocked") && (
              <Spinner size={"xs"} marginLeft={1} />
            )}
          </>
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
    defaultSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    defaultFilters: [],
    storageKey: "views"
  });
  const history = useHistory();
  return (
    <Stack>
      <TableControl<LockableViewDescription>
        table={tableInstance}
        columns={TABLE_COLUMNS}
      />
      <ReactTable<LockableViewDescription>
        table={tableInstance}
        emptyStateMessage="No views found"
        onRowSelect={row => {
          history.push(`/view/${encodeURIComponent(row.original.name)}`);
        }}
      />
    </Stack>
  );
};
