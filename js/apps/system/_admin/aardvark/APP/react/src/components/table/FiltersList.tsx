import { Badge, Button, Grid } from "@chakra-ui/react";
import { Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import { FilterBar } from "./FilterBar";

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
  const columnFilters = table.getState().columnFilters;
  return (
    <Grid gridTemplateColumns="100px 1fr" gap="4">
      <FilterButton
        appliedFiltersCount={columnFilters.length}
        onClick={() => setShowFilters(!showFilters)}
        isSelected={showFilters}
      />
      <FilterBar<Data>
        filters={filters}
        table={table}
        showFilters={showFilters}
        initialFilters={filters.filter(filter => {
          return columnFilters.find(
            columnFilter => columnFilter.id === filter.id
          );
        })}
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
