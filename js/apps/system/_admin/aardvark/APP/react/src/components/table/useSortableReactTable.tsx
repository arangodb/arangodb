import {
  ColumnDef,
  ColumnFiltersState,
  getCoreRowModel,
  getFacetedRowModel,
  getFacetedUniqueValues,
  getFilteredRowModel,
  getSortedRowModel,
  RowSelectionState,
  SortingState,
  TableOptions,
  useReactTable
} from "@tanstack/react-table";
import React from "react";

type SortableReactTableOptions<Data extends object> = {
  data: Data[];
  initialSorting?: SortingState;
  columns: ColumnDef<Data, any>[];
  initialFilters?: ColumnFiltersState;
  initialRowSelection?: RowSelectionState;
};

export const useSortableReactTable = <Data extends object>({
  data,
  columns,
  initialSorting = [],
  initialFilters = [],
  initialRowSelection = {},
  ...rest
}: SortableReactTableOptions<Data> &
  Omit<TableOptions<Data>, "getCoreRowModel">) => {
  const [sorting, setSorting] = React.useState<SortingState>(initialSorting);
  const [columnFilters, setColumnFilters] =
    React.useState<ColumnFiltersState>(initialFilters);
  const [rowSelection, setRowSelection] =
    React.useState<RowSelectionState>(initialRowSelection);
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
      columnFilters,
      rowSelection
    },
    onRowSelectionChange: setRowSelection,
    onColumnFiltersChange: setColumnFilters,
    ...rest
  });
  return table;
};
