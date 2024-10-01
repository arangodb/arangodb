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
  Updater,
  useReactTable,
  VisibilityState
} from "@tanstack/react-table";
import React from "react";

type SortableReactTableOptions<Data extends object> = {
  data: Data[];
  defaultSorting?: SortingState;
  columns: ColumnDef<Data, any>[];
  defaultFilters?: ColumnFiltersState;
  initialRowSelection?: RowSelectionState;
  storageKey?: string;
};

const useParsedValue = (storedValue: any, defaultValue: any) => {
  try {
    const item = JSON.parse(storedValue);
    if (item) {
      return item;
    }
  } catch (e) {
    // ignore
    return defaultValue;
  }
  return defaultValue;
};

const useTableStorage = ({
  storageKey,
  defaultFilters = [],
  defaultColumnVisibility = {}
}: {
  storageKey?: string;
  defaultFilters?: ColumnFiltersState;
  defaultColumnVisibility?: VisibilityState;
}) => {
  const initialColumnFilters = useParsedValue(
    localStorage.getItem(storageKey + "-filters"),
    defaultFilters
  );

  const initialColumnVisibility = useParsedValue(
    localStorage.getItem(storageKey + "-column-visibility"),
    defaultColumnVisibility
  );
  const [columnVisibilityState, setColumnVisibilityState] =
    React.useState<VisibilityState>(initialColumnVisibility);

  const [columnFiltersState, setColumnFiltersState] =
    React.useState<ColumnFiltersState>(initialColumnFilters);
  return {
    columnFilters: columnFiltersState,
    columnVisibility: columnVisibilityState,
    setColumnVisibility: (
      columnVisibilityUpdater: Updater<VisibilityState>
    ) => {
      const finalColumnVisibility =
        typeof columnVisibilityUpdater === "function"
          ? columnVisibilityUpdater(columnVisibilityState)
          : columnVisibilityUpdater;
      setColumnVisibilityState(finalColumnVisibility);
      if (storageKey) {
        localStorage.setItem(
          storageKey + "-column-visibility",
          JSON.stringify(finalColumnVisibility)
        );
      }
    },
    setColumnFilters: (columnFilters: Updater<ColumnFiltersState>) => {
      const finalColumnFilters =
        typeof columnFilters === "function"
          ? columnFilters(columnFiltersState)
          : columnFilters;
      setColumnFiltersState(finalColumnFilters);
      if (storageKey) {
        localStorage.setItem(
          storageKey + "-filters",
          JSON.stringify(finalColumnFilters)
        );
      }
    }
  };
};
export const useSortableReactTable = <Data extends object>({
  data,
  columns,
  defaultSorting = [],
  defaultFilters = [],
  initialRowSelection = {},
  storageKey,
  ...rest
}: SortableReactTableOptions<Data> &
  Omit<TableOptions<Data>, "getCoreRowModel">) => {
  const {
    columnFilters,
    columnVisibility,
    setColumnVisibility,
    setColumnFilters
  } = useTableStorage({
    storageKey,
    defaultFilters,
    defaultColumnVisibility: columns.reduce((acc, column) => {
      const columnId = column.id || "";
      (acc as VisibilityState)[columnId] = true;
      return acc;
    }, {})
  });
  const [sorting, setSorting] = React.useState<SortingState>(defaultSorting);
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
      rowSelection,
      columnVisibility
    },
    onColumnVisibilityChange: (columnVisibility: Updater<VisibilityState>) => {
      setColumnVisibility(columnVisibility);
    },
    onRowSelectionChange: setRowSelection,
    onColumnFiltersChange: (columnFilters: Updater<ColumnFiltersState>) => {
      setColumnFilters(columnFilters);
    },
    ...rest
  });
  return table;
};
