import { ChevronDownIcon, ChevronUpIcon } from "@chakra-ui/icons";
import {
  Box,
  Flex,
  Stack,
  StackProps,
  Table,
  TableContainer,
  TableContainerProps,
  Tbody,
  Td,
  Text,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import {
  Cell,
  flexRender,
  Header,
  Row,
  Table as TableType
} from "@tanstack/react-table";
import * as React from "react";

interface ReactTableProps<Data extends object> extends StackProps {
  table: TableType<Data>;
  emptyStateMessage?: string;
  onCellClick?: (cell: Cell<Data, unknown>) => void;
  onRowSelect?: (row: Row<Data>) => void;
  children?: React.ReactNode;
  renderSubComponent?: (row: Row<Data>) => React.ReactNode;
  layout?: "fixed" | "auto";
  getCellProps?: (cell: Cell<Data, unknown>) => any;
  backgroundColor?: string;
  tableWidth?: string;
  tableContainerProps?: TableContainerProps;
}

export function ReactTable<Data extends object>({
  emptyStateMessage = "No data found.",
  onRowSelect,
  children,
  table,
  renderSubComponent,
  layout,
  onCellClick,
  getCellProps,
  tableWidth,
  backgroundColor = "white",
  tableContainerProps,
  ...rest
}: ReactTableProps<Data>) {
  const rows = table.getRowModel().rows;

  return (
    <Stack {...rest}>
      {children}
      <TableContainer
        borderY="1px solid"
        borderColor="gray.200"
        backgroundColor={backgroundColor}
        {...tableContainerProps}
      >
        <Table
          whiteSpace="normal"
          size="sm"
          colorScheme="gray"
          css={{
            tableLayout: layout
          }}
          width={tableWidth}
        >
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
                <>
                  <SelectableTr<Data>
                    key={row.id}
                    row={row}
                    onRowSelect={onRowSelect}
                    onCellClick={onCellClick}
                    getCellProps={getCellProps}
                  />
                  <Tr>
                    {row.getIsExpanded() && renderSubComponent
                      ? renderSubComponent(row)
                      : null}
                  </Tr>
                </>
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
      width={header.getSize()}
      onClick={header.column.getToggleSortingHandler()}
      _hover={
        canSort
          ? {
              cursor: "pointer"
            }
          : {}
      }
      height={canSort ? "36px" : ""}
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
  onRowSelect,
  onCellClick,
  getCellProps
}: {
  row: Row<Data>;
  onRowSelect: ((row: Row<Data>) => void) | undefined;
  onCellClick?: (cell: Cell<Data, unknown>) => void;
  getCellProps?: (cell: Cell<Data, unknown>) => any;
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
          <Td
            key={cell.id}
            width={cell.column.getSize()}
            onClick={() => onCellClick?.(cell)}
            {...getCellProps?.(cell)}
          >
            {flexRender(cell.column.columnDef.cell, cell.getContext())}
          </Td>
        );
      })}
    </Tr>
  );
};
