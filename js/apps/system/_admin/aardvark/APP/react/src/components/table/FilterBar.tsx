import { AddIcon } from "@chakra-ui/icons";
import {
  Button,
  Grid,
  IconButton,
  Menu,
  MenuButton,
  MenuItem,
  MenuList} from "@chakra-ui/react";
import { Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import { FilterComponent } from "./FilterComponent";
import { FilterType } from "./FiltersList";

export const FilterBar = <Data extends object>({
  table,
  showFilters,
  filters,
  initialFilters = []
}: {
  table: TableType<Data>;
  showFilters: boolean;
  filters: FilterType[];
  initialFilters: (FilterType | undefined)[];
}) => {
  const [currentFilters, setCurrentFilters] =
    React.useState<(FilterType | undefined)[]>(initialFilters);
  const isAnyFilterSelected = currentFilters.length > 0;
  if (!showFilters) {
    return null;
  }
  const filterOptions = filters.filter(filter => {
    return !currentFilters.find(
      currentFilter => currentFilter?.id === filter.id
    );
  });
  const addFilter = (filter: FilterType) => {
    const filterId = filter.id;
    const column = table.getColumn(filterId);
    if (!column || !column.getCanFilter()) {
      return;
    }
    const newCurrentFilters = [
      ...currentFilters,
      filters.find(filter => filter.id === filterId) as FilterType
    ];
    setCurrentFilters(newCurrentFilters);
  };
  return (
    <Grid
      gridColumn="1 / span 3"
      gridTemplateColumns="repeat(auto-fill, minmax(200px, 1fr))"
      gap="4"
    >
      {currentFilters.map(filter => {
        if (!filter) {
          return null;
        }
        const column = table.getColumn(filter.id);
        if (!column) {
          return null;
        }
        return (
          <FilterComponent<Data>
            filter={filter}
            currentFilters={currentFilters}
            setCurrentFilters={setCurrentFilters}
            table={table}
            column={column}
            key={filter.id}
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
