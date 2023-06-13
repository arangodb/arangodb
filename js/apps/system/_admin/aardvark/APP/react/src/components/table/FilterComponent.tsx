import { MinusIcon } from "@chakra-ui/icons";
import { Flex, FormLabel, IconButton, Stack, Input } from "@chakra-ui/react";
import { Column, Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import MultiSelect from "../select/MultiSelect";
import SingleSelect from "../select/SingleSelect";
import { FilterType } from "./FiltersList";

export const FilterComponent = <Data extends object>({
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
  const { options, columnFilterValue } = useFilterValues<Data>({
    table,
    column,
    filter
  });
  return (
    <Flex alignItems="flex-end">
      <Stack width="full">
        <FilterLabel<Data>
          column={column}
          currentFilters={currentFilters}
          filter={filter}
          setCurrentFilters={setCurrentFilters}
        />
        <FilterInput<Data>
          filter={filter}
          column={column}
          options={options}
          columnFilterValue={columnFilterValue}
        />
      </Stack>
    </Flex>
  );
};
const FilterLabel = <Data extends object>({
  column,
  currentFilters,
  filter,
  setCurrentFilters
}: {
  column: Column<Data, unknown>;
  currentFilters: (FilterType | undefined)[];
  filter: FilterType;
  setCurrentFilters: (filters: (FilterType | undefined)[]) => void;
}) => {
  return (
    <Stack direction="row" alignItems="center">
      <FormLabel margin="0" htmlFor={column.id}>
        {column.columnDef.header}
      </FormLabel>
      <IconButton
        size="xxs"
        borderRadius="full"
        variant="outline"
        icon={<MinusIcon boxSize="2" />}
        padding="1"
        aria-label={"Remove filter"}
        colorScheme="red"
        onClick={() => {
          const newCurrentFilters = currentFilters.filter(
            currentFilter => currentFilter?.id !== filter.id
          );
          setCurrentFilters(newCurrentFilters);
          column.setFilterValue(undefined);
        }}
      />
    </Stack>
  );
};

const useFilterValues = <Data extends object>({
  table,
  column,
  filter
}: {
  table: TableType<Data>;
  column: Column<Data, unknown>;
  filter: FilterType;
}) => {
  const firstValue = table
    .getPreFilteredRowModel()
    .flatRows[0]?.getValue(column.id);

  const columnFilterValue = column.getFilterValue() as
    | string
    | string[]
    | undefined;
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
  const options = sortedUniqueValues.map(value => ({
    label: value,
    value
  }));
  return { options, columnFilterValue };
};
const FilterInput = <Data extends object>({
  filter,
  column,
  options,
  columnFilterValue
}: {
  filter: FilterType;
  column: Column<Data, unknown>;
  options: { label: any; value: any }[];
  columnFilterValue: string | string[] | undefined;
}) => {
  if (filter.filterType === "text") {
    return (
      <Input
        placeholder="Search"
        id={column.id}
        value={columnFilterValue}
        onChange={event => {
          column.setFilterValue(event.target.value);
        }}
      />
    );
  }
  if (filter.filterType === "multi-select") {
    return (
      <MultiSelect
        inputId={column.id}
        isClearable
        options={options}
        value={
          columnFilterValue && Array.isArray(columnFilterValue)
            ? columnFilterValue.map(value => ({
                label: value,
                value
              }))
            : undefined
        }
        onChange={(value, action) => {
          if (action.action === "clear") {
            column.setFilterValue([]);
            return;
          }
          column.setFilterValue(value?.map(option => option.value));
        }}
      />
    );
  }
  return (
    <SingleSelect
      inputId={column.id}
      isClearable
      options={options}
      value={
        columnFilterValue && typeof columnFilterValue === "string"
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
  );
};
