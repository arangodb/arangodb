import {
  Badge,
  Button,
  Flex,
  Popover,
  PopoverArrow,
  PopoverBody,
  PopoverCloseButton,
  PopoverContent,
  PopoverHeader,
  PopoverTrigger,
  Stack
} from "@chakra-ui/react";
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
    <Popover
      onClose={() => {
        setShowFilters(false);
      }}
      isOpen={showFilters}
      flip
      placement="top-start"
    >
      <PopoverTrigger>
        <Flex>
          <FilterButton
            appliedFiltersCount={columnFilters.length}
            onClick={() => setShowFilters(!showFilters)}
            isSelected={showFilters}
          />
        </Flex>
      </PopoverTrigger>
      <PopoverContent width="520px" padding="2">
        <PopoverArrow />
        <PopoverCloseButton top="3" />
        <PopoverHeader>Filters</PopoverHeader>
        <PopoverBody>
          <Flex>
            <FilterBar<Data>
              columns={columns}
              table={table}
              showFilters={showFilters}
              currentFilterColumns={currentFilterColumns}
              setCurrentFilterColumns={setCurrentFilterColumns}
              appliedFiltersCount={columnFilters.length}
              onClear={() => {
                table.resetColumnFilters();
                setCurrentFilterColumns([]);
              }}
            />
          </Flex>
        </PopoverBody>
      </PopoverContent>
    </Popover>
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
    <Stack direction="row" alignItems="center">
      <Button
        onClick={onClick}
        backgroundColor={isSelected ? "gray.200" : "white"}
        borderColor="gray.900"
        variant={"outline"}
        size="sm"
      >
        Filters
        <FilterCountBadge appliedFiltersCount={appliedFiltersCount} />
      </Button>
    </Stack>
  );
};

const FilterCountBadge = ({
  appliedFiltersCount
}: {
  appliedFiltersCount: number | undefined;
}) => {
  if (!appliedFiltersCount) {
    return null;
  }
  return (
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
  );
};
