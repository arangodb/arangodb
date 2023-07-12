import {
  ColumnDef,
  ColumnFiltersState,
  getCoreRowModel,
  getFacetedRowModel,
  getFacetedUniqueValues,
  getFilteredRowModel,
  getSortedRowModel,
  SortingState,
  useReactTable
} from "@tanstack/react-table";
import React from "react";

type SortableReactTableOptions<Data extends object> = {
  data: Data[];
  initialSorting?: SortingState;
  columns: ColumnDef<Data, any>[];
  initialFilters?: ColumnFiltersState;
};

export const useSortableReactTable = <Data extends object>({
  data,
  columns,
  initialSorting = [],
  initialFilters = []
}: SortableReactTableOptions<Data>) => {
  const [sorting, setSorting] = React.useState<SortingState>(initialSorting);
  const [columnFilters, setColumnFilters] =
    React.useState<ColumnFiltersState>(initialFilters);
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
  return table;
};
