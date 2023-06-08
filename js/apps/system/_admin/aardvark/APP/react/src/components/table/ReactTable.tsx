import { TriangleDownIcon, TriangleUpIcon } from "@chakra-ui/icons";
import {
  Box,
  Stack,
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import {
  ColumnDef,
  ColumnFiltersState,
  flexRender,
  getCoreRowModel,
  getFacetedRowModel,
  getFacetedUniqueValues,
  getFilteredRowModel,
  getSortedRowModel,
  Row,
  SortingState,
  Table as TableType,
  useReactTable
} from "@tanstack/react-table";
import * as React from "react";

type ReactTableProps<Data extends object> = {
  data: Data[];
  initialSorting?: SortingState;
  columns: ColumnDef<Data, any>[];
  emptyStateMessage?: string;
  onRowSelect?: (row: Row<Data>) => void;
  children?: ({ table }: { table: TableType<Data> }) => React.ReactNode;
};

export function ReactTable<Data extends object>({
  data,
  columns,
  initialSorting = [],
  emptyStateMessage = "No data found.",
  onRowSelect,
  children
}: ReactTableProps<Data>) {
  const [sorting, setSorting] = React.useState<SortingState>(initialSorting);
  const [columnFilters, setColumnFilters] = React.useState<ColumnFiltersState>(
    []
  );
  const table = useReactTable({
    columns,
    data,
    getCoreRowModel: getCoreRowModel(),
    onSortingChange: setSorting,
    getSortedRowModel: getSortedRowModel(),
    getFilteredRowModel: getFilteredRowModel(),
    getFacetedUniqueValues: getFacetedUniqueValues(),
    getFacetedRowModel: getFacetedRowModel(),
    state: {
      sorting,
      columnFilters
    },
    onColumnFiltersChange: setColumnFilters
  });
  const rows = table.getRowModel().rows;
  return (
    <Stack>
      {children && children({ table })}
      <TableContainer
        border="1px solid"
        borderColor="gray.200"
        backgroundColor="white"
      >
        <Table whiteSpace="normal" size="sm" colorScheme="gray">
          <Thead>
            {table.getHeaderGroups().map(headerGroup => (
              <Tr key={headerGroup.id}>
                {headerGroup.headers.map(header => {
                  return (
                    <Th
                      key={header.id}
                      onClick={header.column.getToggleSortingHandler()}
                    >
                      <Box as="span" display="flex">
                        {flexRender(
                          header.column.columnDef.header,
                          header.getContext()
                        )}
                        <Box as="span" paddingLeft="2">
                          {header.column.getIsSorted() ? (
                            header.column.getIsSorted() === "desc" ? (
                              <TriangleDownIcon aria-label="sorted descending" />
                            ) : (
                              <TriangleUpIcon aria-label="sorted ascending" />
                            )
                          ) : null}
                        </Box>
                      </Box>
                    </Th>
                  );
                })}
              </Tr>
            ))}
          </Thead>
          <Tbody>
            {rows.length > 0 ? (
              rows.map(row => (
                <Tr
                  key={row.id}
                  onClick={() => onRowSelect?.(row)}
                  _hover={
                    onRowSelect
                      ? {
                          cursor: "pointer",
                          backgroundColor: "gray.50"
                        }
                      : {}
                  }
                >
                  {row.getVisibleCells().map(cell => {
                    return (
                      <Td key={cell.id}>
                        {flexRender(
                          cell.column.columnDef.cell,
                          cell.getContext()
                        )}
                      </Td>
                    );
                  })}
                </Tr>
              ))
            ) : (
              <Box padding="4">{emptyStateMessage}</Box>
            )}
          </Tbody>
        </Table>
      </TableContainer>
    </Stack>
  );
}
