import { AddIcon } from "@chakra-ui/icons";
import {
  Button,
  Flex,
  Menu,
  MenuButton,
  MenuItem,
  MenuList
} from "@chakra-ui/react";
import { ColumnDef, Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import { FilterComponent } from "./FilterComponent";

export const FilterBar = <Data extends object>({
  table,
  showFilters,
  columns,
  currentFilterColumns,
  setCurrentFilterColumns,
  appliedFiltersCount,
  onClear
}: {
  table: TableType<Data>;
  showFilters: boolean;
  columns: ColumnDef<Data>[];
  currentFilterColumns: (ColumnDef<Data> | undefined)[];
  appliedFiltersCount: number;
  onClear: () => void;
  setCurrentFilterColumns: (
    currentFilterColumns: (ColumnDef<Data> | undefined)[]
  ) => void;
}) => {
  if (!showFilters) {
    return null;
  }
  const filterOptions = columns.filter(column => {
    if (column.enableColumnFilter === false) {
      return false;
    }
    return !currentFilterColumns.find(
      currentFilter => currentFilter?.id === column.id
    );
  });
  const addFilter = (filter: ColumnDef<Data>) => {
    const filterId = filter.id;
    const column = filterId && table.getColumn(filterId);
    if (!column || !column.getCanFilter()) {
      return;
    }
    const newCurrentFilterColumns = [
      ...currentFilterColumns,
      columns.find(column => column.id === filterId)
    ];
    setCurrentFilterColumns(newCurrentFilterColumns);
  };
  return (
    <Flex direction="column" gap="4" width="full">
      <Flex gap="2" direction="column">
        {currentFilterColumns.map(column => {
          if (!column || !column.id) {
            return null;
          }
          return (
            <FilterComponent<Data>
              column={column}
              currentFilterColumns={currentFilterColumns}
              setCurrentFilterColumns={setCurrentFilterColumns}
              table={table}
              key={column.id}
            />
          );
        })}
      </Flex>
      <Flex alignItems="center" justifyContent="space-between">
        <AddFilterButton<Data>
          filterOptions={filterOptions}
          table={table}
          addFilter={addFilter}
        />
        <ClearButton
          onClear={onClear}
          appliedFiltersCount={appliedFiltersCount}
        />
      </Flex>
    </Flex>
  );
};
const ClearButton = ({
  onClear,
  appliedFiltersCount
}: {
  onClear: () => void;
  appliedFiltersCount: number;
}) => {
  return (
    <Button
      textTransform="uppercase"
      onClick={() => {
        onClear();
      }}
      variant="ghost"
      size="xs"
      colorScheme="blue"
      isDisabled={appliedFiltersCount === 0}
    >
      Clear
    </Button>
  );
};

const AddFilterButton = <Data extends object>({
  filterOptions,
  table,
  addFilter
}: {
  filterOptions: ColumnDef<Data, unknown>[];
  table: TableType<Data>;
  addFilter: (filter: ColumnDef<Data, unknown>) => void;
}) => {
  if (filterOptions.length === 0) {
    return null;
  }
  return (
    <Menu>
      <MenuButton
        as={Button}
        size="xs"
        height="30px"
        alignSelf={"flex-end"}
        marginBottom="1"
        leftIcon={<AddIcon />}
        aria-label={"Add filter"}
        width="fit-content"
      >
        Add filter
      </MenuButton>
      <MenuList>
        {filterOptions.map(filter => {
          const column = filter.id && table.getColumn(filter.id);
          const header = table
            .getFlatHeaders()
            .find(header => header.id === filter.id);
          if (!column || !column.getCanFilter() || !header) {
            return null;
          }
          return (
            <MenuItem onClick={() => addFilter(filter)} key={filter.id}>
              {typeof filter.header === "function"
                ? filter.header({
                    column,
                    header,
                    table
                  })
                : filter.header}
            </MenuItem>
          );
        })}
      </MenuList>
    </Menu>
  );
};
