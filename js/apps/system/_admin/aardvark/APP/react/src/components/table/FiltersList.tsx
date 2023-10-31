import { Badge, Button, Grid, Stack } from "@chakra-ui/react";
import { ColumnDef, Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import { FilterBar } from "./FilterBar";

export const FiltersList = <Data extends object>({
  table,
  columns
}: {
  table: TableType<Data>;
  columns: ColumnDef<Data, any>[];
}) => {
  const [showFilters, setShowFilters] = React.useState(false);
  const columnFilters = table.getState().columnFilters;
  const initialFilterColumns = columns.filter(column => {
    return columnFilters.find(columnFilter => columnFilter.id === column.id);
  });
  const [currentFilterColumns, setCurrentFilterColumns] =
    React.useState<(ColumnDef<Data> | undefined)[]>(initialFilterColumns);
  return (
    <Grid gridTemplateColumns="200px 1fr" gap="4">
      <FilterButton
        appliedFiltersCount={columnFilters.length}
        onClick={() => setShowFilters(!showFilters)}
        isSelected={showFilters}
        onReset={() => {
          table.resetColumnFilters();
          setCurrentFilterColumns([]);
        }}
      />
      <FilterBar<Data>
        columns={columns}
        table={table}
        showFilters={showFilters}
        currentFilterColumns={currentFilterColumns}
        setCurrentFilterColumns={setCurrentFilterColumns}
      />
    </Grid>
  );
};

const FilterButton = ({
  onClick,
  isSelected,
  appliedFiltersCount,
  onReset
}: {
  onClick: () => void;
  isSelected: boolean;
  appliedFiltersCount: number;
  onReset: () => void;
}) => {
  return (
    <Stack direction="row" alignItems="center">
      <Button
        onClick={onClick}
        backgroundColor={isSelected ? "gray.200" : "white"}
        borderColor="gray.900"
        variant={"outline"}
        size="sm"
      >
        Filters
        {appliedFiltersCount > 0 && (
          <Badge
            fontSize="xs"
            width="4"
            height="4"
            display="flex"
            alignItems="center"
            justifyContent="center"
            position="absolute"
            right="-8px"
            top="-8px"
            backgroundColor="gray.900"
            color="white"
            borderRadius="20px"
          >
            {appliedFiltersCount}
          </Badge>
        )}
      </Button>
      {appliedFiltersCount > 0 && (
        <Button
          textTransform="uppercase"
          onClick={() => {
            onReset();
          }}
          variant="ghost"
          size="xs"
          colorScheme="blue"
        >
          Reset
        </Button>
      )}
    </Stack>
  );
};
