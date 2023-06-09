import { TriangleDownIcon, TriangleUpIcon } from "@chakra-ui/icons";
import {
  Box,
  Flex,
  Stack,
  Table,
  TableContainer,
  Tbody,
  Td,
  Text,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import {
  flexRender,
  Header,
  Row,
  Table as TableType
} from "@tanstack/react-table";
import * as React from "react";

type ReactTableProps<Data extends object> = {
  table: TableType<Data>;
  emptyStateMessage?: string;
  onRowSelect?: (row: Row<Data>) => void;
  children?: React.ReactNode;
};

export function ReactTable<Data extends object>({
  emptyStateMessage = "No data found.",
  onRowSelect,
  children,
  table
}: ReactTableProps<Data>) {
  const rows = table.getRowModel().rows;
  return (
    <Stack>
      {children}
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
                  return <SortableTh<Data> header={header} />;
                })}
              </Tr>
            ))}
          </Thead>
          <Tbody>
            {rows.length > 0 ? (
              rows.map(row => (
                <SelectableTr<Data> row={row} onRowSelect={onRowSelect} />
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

const SortableTh = <Data extends object>({
  header
}: {
  header: Header<Data, unknown>;
}) => {
  const canSort = header.column.getCanSort();
  return (
    <Th
      key={header.id}
      onClick={header.column.getToggleSortingHandler()}
      _hover={
        canSort
          ? {
              cursor: "pointer"
            }
          : {}
      }
    >
      <Box display="flex" alignItems="center">
        <Text>
          {flexRender(header.column.columnDef.header, header.getContext())}
        </Text>
        {canSort && (
          <Flex marginLeft="2" height={3} width={3}>
            {header.column.getIsSorted() ? (
              header.column.getIsSorted() === "desc" ? (
                <TriangleDownIcon
                  width={3}
                  height={3}
                  aria-label="sorted descending"
                />
              ) : (
                <TriangleUpIcon
                  width={3}
                  height={3}
                  aria-label="sorted ascending"
                />
              )
            ) : null}
          </Flex>
        )}
      </Box>
    </Th>
  );
};

const SelectableTr = <Data extends object>({
  row,
  onRowSelect
}: {
  row: Row<Data>;
  onRowSelect: ((row: Row<Data>) => void) | undefined;
}) => {
  return (
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
            {flexRender(cell.column.columnDef.cell, cell.getContext())}
          </Td>
        );
      })}
    </Tr>
  );
};
