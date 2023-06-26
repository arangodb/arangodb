import { AddIcon } from "@chakra-ui/icons";
import {
  Button,
  Grid,
  IconButton,
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
  setCurrentFilterColumns
}: {
  table: TableType<Data>;
  showFilters: boolean;
  columns: ColumnDef<Data>[];
  currentFilterColumns: (ColumnDef<Data> | undefined)[];
  setCurrentFilterColumns: (
    currentFilterColumns: (ColumnDef<Data> | undefined)[]
  ) => void;
}) => {
  const isAnyFilterSelected = currentFilterColumns.length > 0;
  if (!showFilters) {
    return null;
  }
  const filterOptions = columns.filter(column => {
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
    <Grid
      gridColumn="1 / span 3"
      gridTemplateColumns="repeat(auto-fill, minmax(200px, 1fr))"
      gap="4"
    >
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
      {filterOptions.length > 0 && (
        <Menu>
          <MenuButton
            as={isAnyFilterSelected ? IconButton : Button}
            size="sm"
            height="30px"
            alignSelf={"flex-end"}
            marginBottom="1"
            leftIcon={isAnyFilterSelected ? undefined : <AddIcon />}
            icon={isAnyFilterSelected ? <AddIcon /> : undefined}
            aria-label={"Add filter"}
            width="fit-content"
          >
            {isAnyFilterSelected ? "" : "Add filter"}
          </MenuButton>
          <MenuList>
            {filterOptions.map(filter => {
              return (
                <MenuItem onClick={() => addFilter(filter)} key={filter.id}>
                  {filter.header}
                </MenuItem>
              );
            })}
          </MenuList>
        </Menu>
      )}
    </Grid>
  );
};
