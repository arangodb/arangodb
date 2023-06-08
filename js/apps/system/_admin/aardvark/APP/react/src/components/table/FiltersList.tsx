import { AddIcon, CloseIcon } from "@chakra-ui/icons";
import {
  Badge,
  Button,
  Flex,
  FormLabel,
  Grid,
  IconButton,
  Menu,
  MenuButton,
  MenuItem,
  MenuList,
  Stack
} from "@chakra-ui/react";
import { Column, Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import SingleSelect from "../select/SingleSelect";

const HeaderFilter = <Data extends object>({
  filter,
  table,
  column,
  currentFilters,
  setCurrentFilters
}: {
  filter: FilterType;
  table: TableType<Data>;
  column: Column<Data, unknown>;
  currentFilters: (FilterType | undefined)[];
  setCurrentFilters: (filters: (FilterType | undefined)[]) => void;
}) => {
  const firstValue = table
    .getPreFilteredRowModel()
    .flatRows[0]?.getValue(column.id);

  const columnFilterValue = column.getFilterValue() as string | undefined;
  const sortedUniqueValues = React.useMemo(
    () =>
      typeof firstValue === "number"
        ? []
        : [
            ...new Set(
              Array.from(column.getFacetedUniqueValues().keys())
                .sort()
                .map(value => {
                  return filter.getValue ? filter.getValue(value) : value;
                })
            )
          ],
    [column, firstValue, filter]
  );
  console.log({ column });
  const options = sortedUniqueValues.map(value => ({
    label: value,
    value
  }));
  return (
    <Flex alignItems="flex-end">
      <Stack width="full">
        <FormLabel paddingRight="2" htmlFor={column.id}>
          {column.columnDef.header}
        </FormLabel>
        <SingleSelect
          inputId={column.id}
          isClearable
          options={options}
          value={
            columnFilterValue
              ? { label: columnFilterValue, value: columnFilterValue }
              : undefined
          }
          onChange={(value, action) => {
            if (action.action === "clear") {
              column.setFilterValue(undefined);
              return;
            }
            column.setFilterValue(value?.value);
          }}
        />
      </Stack>
      <IconButton
        marginLeft="2"
        marginBottom="2"
        size="xs"
        variant="ghost"
        icon={<CloseIcon />}
        aria-label={"Remove filter"}
        onClick={() => {
          const newCurrentFilters = currentFilters.filter(
            currentFilter => currentFilter?.id !== filter.id
          );
          setCurrentFilters(newCurrentFilters);
          column.setFilterValue(undefined);
        }}
      />
    </Flex>
  );
};

export type FilterType = {
  id: string;
  header: string;
  filterType: "text" | "single-select" | "multi-select";
  getValue?: (value: string) => string;
};
export const FiltersList = <Data extends object>({
  table,
  filters
}: {
  table: TableType<Data>;
  filters: FilterType[];
}) => {
  const [showFilters, setShowFilters] = React.useState(false);
  return (
    <Grid gridTemplateColumns="100px 1fr" gap="4">
      <FilterButton
        appliedFiltersCount={table.getState().columnFilters.length}
        onClick={() => setShowFilters(!showFilters)}
        isSelected={showFilters}
      />
      <FilterSelector<Data>
        filters={filters}
        table={table}
        showFilters={showFilters}
      />
    </Grid>
  );
};

const FilterButton = ({
  onClick,
  isSelected,
  appliedFiltersCount
}: {
  onClick: () => void;
  isSelected: boolean;
  appliedFiltersCount: number;
}) => {
  return (
    <Button
      onClick={onClick}
      backgroundColor={isSelected ? "gray.200" : "white"}
      borderColor="gray.900"
      variant={"outline"}
    >
      Filters
      {appliedFiltersCount > 0 && (
        <Badge
          fontSize="sm"
          width="5"
          height="5"
          display="flex"
          alignItems="center"
          justifyContent="center"
          position="absolute"
          right="-10px"
          top="-10px"
          backgroundColor="gray.900"
          color="white"
          borderRadius="20px"
        >
          {appliedFiltersCount}
        </Badge>
      )}
    </Button>
  );
};

const FilterSelector = <Data extends object>({
  table,
  showFilters,
  filters
}: {
  table: TableType<Data>;
  showFilters: boolean;
  filters: FilterType[];
}) => {
  const [currentFilters, setCurrentFilters] = React.useState<
    (FilterType | undefined)[]
  >([]);
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
          <HeaderFilter<Data>
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
            as={IconButton}
            size="sm"
            width="30px"
            height="30px"
            alignSelf={"flex-end"}
            marginBottom="1"
            icon={<AddIcon />}
            aria-label={"Add filter"}
          />
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
