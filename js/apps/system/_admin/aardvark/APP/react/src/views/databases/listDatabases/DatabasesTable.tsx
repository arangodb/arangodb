import { Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { Link as RouterLink, useHistory } from "react-router-dom";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { useDatabasesContext } from "../DatabasesContext";
import { DatabaseDescription } from "../Database.types";

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
          to={`/databases/${cellValue}`}
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
      <FiltersList<DatabaseDescription>
        columns={TABLE_COLUMNS}
        table={tableInstance}
      />
      <ReactTable<DatabaseDescription>
        table={tableInstance}
        emptyStateMessage="No databases found"
        onRowSelect={row => {
          history.push(`/databases/${row.original.name}`);
        }}
      />
    </Stack>
  );
};
