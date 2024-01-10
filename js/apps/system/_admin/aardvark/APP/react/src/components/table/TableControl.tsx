import { Flex } from "@chakra-ui/react";
import { ColumnDef, Table as TableType } from "@tanstack/react-table";
import * as React from "react";
import { FiltersList } from "./FiltersList";
import { TableColumnSelector } from "./TableColumnSelector";

export const TableControl = <Data extends object>({
  table,
  columns,
  showColumnSelector = true
}: {
  table: TableType<Data>;
  columns: ColumnDef<Data, any>[];
  showColumnSelector?: boolean;
}) => {
  return (
    <Flex gap="4">
      <FiltersList<Data> columns={columns} table={table} />
      {showColumnSelector && (
        <TableColumnSelector<Data> table={table} columns={columns} />
      )}
    </Flex>
  );
};
