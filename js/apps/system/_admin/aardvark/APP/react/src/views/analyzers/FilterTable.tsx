import { TriangleDownIcon, TriangleUpIcon } from "@chakra-ui/icons";
import {
  Box,
  IconButton,
  Menu,
  MenuButton,
  MenuList,
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr,
  useDisclosure
} from "@chakra-ui/react";
import {
  Column,
  ColumnDef,
  ColumnFiltersState,
  flexRender,
  getCoreRowModel,
  getFacetedRowModel,
  getFacetedUniqueValues,
  getFilteredRowModel,
  getSortedRowModel,
  SortingState,
  Table as TableType,
  useReactTable
} from "@tanstack/react-table";
import * as React from "react";
import { FilterList } from "styled-icons/material";
import SingleSelect from "../../components/select/SingleSelect";

export type FilterTableProps<Data extends object> = {
  data: Data[];
  initialSorting?: SortingState;
  columns: ColumnDef<Data, any>[];
  emptyStateMessage?: string;
};

export function FilterTable<Data extends object>({
  data,
  columns,
  initialSorting = [],
  emptyStateMessage = "No data found."
}: FilterTableProps<Data>) {
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
    <TableContainer
      border="1px solid"
      borderColor="gray.200"
      backgroundColor="white"
    >
      <Table whiteSpace="normal" size="sm" variant="striped" colorScheme="gray">
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

                      {header.column.getCanFilter() && (
                        <Box
                          textTransform="none"
                          fontWeight="normal"
                          letterSpacing={0}
                          as="span"
                          paddingLeft="2"
                          onClick={e => {
                            e.stopPropagation();
                          }}
                        >
                          <FilterMenu<Data>
                            table={table}
                            column={header.column}
                          />
                        </Box>
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
              <Tr key={row.id}>
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
  );
}

const FilterMenu = <Data extends object>({
  table,
  column
}: {
  table: TableType<Data>;
  column: Column<Data, unknown>;
}) => {
  const firstValue = table
    .getPreFilteredRowModel()
    .flatRows[0]?.getValue(column.id);

  const columnFilterValue = column.getFilterValue();

  const sortedUniqueValues = React.useMemo(
    () =>
      typeof firstValue === "number"
        ? []
        : Array.from(column.getFacetedUniqueValues().keys()).sort(),
    [column.getFacetedUniqueValues()]
  );

  const options = sortedUniqueValues.map(value => ({
    label: value,
    value
  }));
  const { onOpen, onClose, isOpen } = useDisclosure();
  return (
    <Menu
      closeOnSelect={false}
      isOpen={isOpen}
      onOpen={onOpen}
      onClose={onClose}
    >
      <MenuButton
        borderRadius="sm"
        as={IconButton}
        variant="ghost"
        size="xxs"
        width="20px"
        height="20px"
        icon={<FilterList />}
        aria-label={"Filter"}
      />
      <MenuList>
        <Box padding={"4"}>
          <SingleSelect
            isClearable
            options={options}
            onChange={(value, action) => {
              if (action.action === "clear") {
                column.setFilterValue(undefined);
                return;
              }

              column.setFilterValue(value?.value);
            }}
          />
        </Box>
      </MenuList>
    </Menu>
  );
};
