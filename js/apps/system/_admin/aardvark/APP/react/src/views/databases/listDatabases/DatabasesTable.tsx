import { Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { Link as RouterLink, useHistory } from "react-router-dom";
import { ReactTable } from "../../../components/table/ReactTable";
import { TableControl } from "../../../components/table/TableControl";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { DatabaseDescription } from "../Database.types";
import { useDatabasesContext } from "../DatabasesContext";

const columnHelper = createColumnHelper<DatabaseDescription>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          as={RouterLink}
          to={`/databases/${encodeURIComponent(cellValue)}`}
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
  })
];

export const DatabasesTable = () => {
  const { databases } = useDatabasesContext();
  const tableInstance = useSortableReactTable<DatabaseDescription>({
    data: databases || [],
    columns: TABLE_COLUMNS,
    defaultSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    defaultFilters: [],
    storageKey: "databases"
  });
  const history = useHistory();
  return (
    <Stack>
      <TableControl<DatabaseDescription>
        columns={TABLE_COLUMNS}
        table={tableInstance}
        showColumnSelector={false}
      />
      <ReactTable<DatabaseDescription>
        table={tableInstance}
        emptyStateMessage="No databases found"
        onRowSelect={row => {
          history.push(`/databases/${encodeURIComponent(row.original.name)}`);
        }}
      />
    </Stack>
  );
};
