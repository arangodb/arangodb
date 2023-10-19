import { ChevronDownIcon, ChevronUpIcon } from "@chakra-ui/icons";
import { Box, Flex, Grid, Text } from "@chakra-ui/react";
import {
  Cell,
  CellContext,
  flexRender,
  Header,
  Row,
  Table as TableType
} from "@tanstack/react-table";
import * as React from "react";

type ReactTableProps<Data extends object> = {
  table: TableType<Data>;
  emptyStateMessage?: string;
  onCellClick?: (cell: Cell<Data, unknown>) => void;
  onRowSelect?: (row: Row<Data>) => void;
  renderSubComponent?: (row: Row<Data>) => React.ReactNode;
  getCellProps?: (
    cell: CellContext<Data, unknown>
  ) => { [key: string]: any } | undefined;
  getRowProps?: (row: Row<Data>) => { [key: string]: any } | undefined;
};

export function ReactTable<Data extends object>({
  emptyStateMessage = "No data found.",
  onRowSelect,
  table,
  renderSubComponent,
  onCellClick,
  getCellProps,
  getRowProps
}: ReactTableProps<Data>) {
  const rows = table.getRowModel().rows;

  return (
    <Grid
      position="relative"
      overflow="auto"
      gridTemplateRows="auto 1fr"
      borderY="1px solid"
      borderColor="gray.200"
      backgroundColor="white"
    >
      <Grid gridAutoFlow="column">
        {table.getHeaderGroups().map(headerGroup => (
          <Grid gridAutoFlow="column" key={headerGroup.id}>
            {headerGroup.headers.map(header => {
              return (
                <SortableTableHeader<Data> key={header.id} header={header} />
              );
            })}
          </Grid>
        ))}
      </Grid>
      <Grid gridAutoFlow="row">
        {rows.length > 0 ? (
          rows.map(row => (
            <>
              <SelectableTableRow<Data>
                getCellProps={getCellProps}
                getRowProps={getRowProps}
                key={row.id}
                row={row}
                onRowSelect={onRowSelect}
                onCellClick={onCellClick}
              />
              <Box>
                {row.getIsExpanded() && renderSubComponent
                  ? renderSubComponent(row)
                  : null}
              </Box>
            </>
          ))
        ) : (
          <Box padding="4">{emptyStateMessage}</Box>
        )}
      </Grid>
    </Grid>
  );
}

const SortableTableHeader = <Data extends object>({
  header
}: {
  header: Header<Data, unknown>;
}) => {
  const canSort = header.column.getCanSort();
  return (
    <Box
      padding="1"
      width={header.column.getSize()}
      paddingY="2"
      fontWeight="500"
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
    </Box>
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

const SelectableTableRow = <Data extends object>({
  row,
  onRowSelect,
  onCellClick,
  getCellProps,
  getRowProps
}: {
  row: Row<Data>;
  onRowSelect: ((row: Row<Data>) => void) | undefined;
  onCellClick?: (cell: Cell<Data, unknown>) => void;
  getCellProps?: (
    cell: CellContext<Data, unknown>
  ) => { [key: string]: any } | undefined;
  getRowProps?: (row: Row<Data>) => { [key: string]: any } | undefined;
}) => {
  return (
    <Grid
      gridAutoFlow="column"
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
      alignItems="center"
      {...getRowProps?.(row)}
    >
      {row.getVisibleCells().map(cell => {
        return (
          <Box
            key={cell.id}
            width={cell.column.getSize()}
            onClick={() => onCellClick?.(cell)}
            padding="1"
            paddingY="2"
            {...getCellProps?.(cell.getContext())}
          >
            {flexRender(cell.column.columnDef.cell, cell.getContext())}
          </Box>
        );
      })}
    </Grid>
  );
};
