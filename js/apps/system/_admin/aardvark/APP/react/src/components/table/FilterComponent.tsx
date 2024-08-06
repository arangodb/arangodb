import { MinusIcon } from "@chakra-ui/icons";
import { FormLabel, Grid, IconButton, Input, Stack } from "@chakra-ui/react";
import { Column, ColumnDef, Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import MultiSelect from "../select/MultiSelect";
import SingleSelect from "../select/SingleSelect";

export const FilterComponent = <Data extends object>({
  column,
  table,
  currentFilterColumns,
  setCurrentFilterColumns
}: {
  column: ColumnDef<Data>;
  table: TableType<Data>;
  currentFilterColumns: (ColumnDef<Data> | undefined)[];
  setCurrentFilterColumns: (filters: (ColumnDef<Data> | undefined)[]) => void;
}) => {
  const columnInstance = column.id ? table.getColumn(column.id) : undefined;
  const { options, columnFilterValue } = useFilterValues<Data>({
    table,
    columnInstance,
    column
  });
  if (!columnInstance) {
    return null;
  }
  return (
    <Grid
      templateColumns="1fr 1fr auto"
      width="full"
      alignItems="center"
      gap="2"
    >
      <FilterLabel<Data> table={table} columnInstance={columnInstance} />
      <FilterInput<Data>
        column={column}
        columnInstance={columnInstance}
        options={options}
        columnFilterValue={columnFilterValue}
      />
      <RemoveFilterButton
        currentFilterColumns={currentFilterColumns}
        column={column}
        setCurrentFilterColumns={setCurrentFilterColumns}
        columnInstance={columnInstance}
      />
    </Grid>
  );
};
const FilterLabel = <Data extends object>({
  columnInstance,
  table
}: {
  columnInstance: Column<Data, unknown>;
  table: TableType<Data>;
}) => {
  const header = table
    .getFlatHeaders()
    .find(header => header.id === columnInstance.id);
  if (!header) {
    return null;
  }
  return (
    <Stack direction="row" alignItems="center">
      <FormLabel margin="0" htmlFor={columnInstance.id}>
        {typeof columnInstance.columnDef.header === "function"
          ? columnInstance.columnDef.header({
              column: columnInstance,
              table: table,
              header
            })
          : columnInstance.columnDef.header}
      </FormLabel>
    </Stack>
  );
};

const useFilterValues = <Data extends object>({
  table,
  columnInstance,
  column
}: {
  table: TableType<Data>;
  columnInstance?: Column<Data, unknown>;
  column: ColumnDef<Data>;
}) => {
  const firstValue =
    columnInstance?.id &&
    table.getPreFilteredRowModel().flatRows[0]?.getValue(columnInstance.id);
  const columnFilterValue = columnInstance?.getFilterValue() as
    | string
    | string[]
    | undefined;
  if (column.meta?.filterType === "text") {
    return { options: [], columnFilterValue };
  }
  let options = [] as {
    label: string;
    value: string;
  }[];
  if (typeof firstValue !== "number") {
    const uniqueValues = columnInstance?.getFacetedUniqueValues() || new Map();
    const sortedUniqueValues = [
      ...new Set(Array.from(uniqueValues.keys()).sort())
    ];
    options = sortedUniqueValues.map(value => ({
      label: value,
      value
    }));
  }
  return { options, columnFilterValue };
};
const FilterInput = <Data extends object>({
  column,
  columnInstance,
  options,
  columnFilterValue
}: {
  column: ColumnDef<Data>;
  columnInstance: Column<Data, unknown>;
  options: { label: any; value: any }[];
  columnFilterValue: string | string[] | undefined;
}) => {
  const filterType = column.meta?.filterType;
  if (filterType === "text") {
    return (
      <Input
        placeholder="Search"
        id={columnInstance.id}
        value={columnFilterValue}
        onChange={event => {
          columnInstance.setFilterValue(event.target.value);
        }}
      />
    );
  }
  if (filterType === "multi-select") {
    return (
      <MultiSelect
        inputId={columnInstance.id}
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
            columnInstance.setFilterValue([]);
            return;
          }
          columnInstance.setFilterValue(value?.map(option => option.value));
        }}
      />
    );
  }
  return (
    <SingleSelect
      inputId={columnInstance.id}
      isClearable
      options={options}
      value={
        columnFilterValue && typeof columnFilterValue === "string"
          ? { label: columnFilterValue, value: columnFilterValue }
          : undefined
      }
      onChange={(value, action) => {
        if (action.action === "clear") {
          columnInstance.setFilterValue(undefined);
          return;
        }
        columnInstance.setFilterValue(value?.value);
      }}
    />
  );
};

const RemoveFilterButton = <Data extends object>({
  currentFilterColumns,
  column,
  setCurrentFilterColumns,
  columnInstance
}: {
  currentFilterColumns: (ColumnDef<Data, unknown> | undefined)[];
  column: ColumnDef<Data, unknown>;
  setCurrentFilterColumns: (
    filters: (ColumnDef<Data, unknown> | undefined)[]
  ) => void;
  columnInstance: Column<Data, unknown>;
}) => {
  return (
    <IconButton
      size="xxs"
      borderRadius="full"
      variant="outline"
      icon={<MinusIcon boxSize="2" />}
      padding="1"
      aria-label={"Remove filter"}
      colorScheme="red"
      onClick={() => {
        const newCurrentFilterColumns = currentFilterColumns.filter(
          currentFilter => currentFilter?.id !== column.id
        );
        setCurrentFilterColumns(newCurrentFilterColumns);
        columnInstance.setFilterValue(undefined);
      }}
    />
  );
};
