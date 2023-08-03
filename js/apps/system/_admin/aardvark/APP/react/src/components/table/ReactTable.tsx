import { ChevronDownIcon, ChevronUpIcon } from "@chakra-ui/icons";
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
        borderY="1px solid"
        borderColor="gray.200"
        backgroundColor="white"
      >
        <Table whiteSpace="normal" size="sm" colorScheme="gray">
          <Thead>
            {table.getHeaderGroups().map(headerGroup => (
              <Tr key={headerGroup.id}>
                {headerGroup.headers.map(header => {
                  return <SortableTh<Data> key={header.id} header={header} />;
                })}
              </Tr>
            ))}
          </Thead>
          <Tbody>
            {rows.length > 0 ? (
              rows.map(row => (
                <SelectableTr<Data>
                  key={row.id}
                  row={row}
                  onRowSelect={onRowSelect}
                />
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
      onClick={header.column.getToggleSortingHandler()}
      _hover={
        canSort
          ? {
              cursor: "pointer"
            }
          : {}
      }
      role="group"
    >
      <Box display="flex" alignItems="center">
        <Text>
          {flexRender(header.column.columnDef.header, header.getContext())}
        </Text>
        {canSort && <SortIcon sortedDirection={header.column.getIsSorted()} />}
      </Box>
    </Th>
  );
};

const SortIcon = ({
  sortedDirection
}: {
  sortedDirection: "desc" | "asc" | false;
}) => {
  const isSorted = sortedDirection !== false;
  const isAscending = sortedDirection === "asc";
  const upIconColor = sortedDirection === "asc" ? "black" : "inherit";
  const downIconColor = sortedDirection === "desc" ? "black" : "inherit";
  return (
    <Flex
      marginLeft="1"
      _groupHover={{
        color: "gray.600"
      }}
      color="gray.400"
      transition="color 0.3s ease"
      direction="column"
    >
      <Box
        display={isAscending || !isSorted ? "block" : "none"}
        color={upIconColor}
        as={ChevronUpIcon}
        width="4"
        height="4"
        aria-label="sorted descending"
      />
      <Box
        display={!isAscending || !isSorted ? "block" : "none"}
        color={downIconColor}
        as={ChevronDownIcon}
        marginTop={!isSorted ? "-2" : "0"}
        width="4"
        height="4"
        aria-label="sorted ascending"
      />
    </Flex>
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
      onClick={() => onRowSelect?.(row)}
      _hover={
        onRowSelect
          ? {
              cursor: "pointer",
              backgroundColor: "gray.50"
            }
          : {}
      }
      backgroundColor={row.getIsSelected() ? "gray.100" : undefined}
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
