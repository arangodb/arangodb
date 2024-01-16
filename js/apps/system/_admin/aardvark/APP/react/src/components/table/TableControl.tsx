import {
  Button,
  Checkbox,
  Flex,
  Menu,
  MenuButton,
  MenuList,
  MenuOptionGroup
} from "@chakra-ui/react";
import { ColumnDef, Table as TableType } from "@tanstack/react-table";
import * as React from "react";

export const TableControl = <Data extends object>({
  table,
  columns
}: {
  table: TableType<Data>;
  columns: ColumnDef<Data, any>[];
}) => {
  // list of columns with a checkbox to toggle visibility
  return (
    <Menu closeOnSelect={false}>
      <MenuButton
        as={Button}
        borderColor="gray.900"
        variant="outline"
        size="sm"
      >
        Columns
      </MenuButton>
      <MenuList minWidth="auto">
        <MenuOptionGroup defaultChecked type="checkbox">
          <Flex padding="2" gap="2" direction="column">
            {columns.map(column => {
              const tableColumn = column.id
                ? table.getColumn(column.id)
                : undefined;
              const isVisible = tableColumn?.getIsVisible();
              return (
                <Checkbox
                  defaultChecked={isVisible}
                  onChange={() => {
                    tableColumn?.toggleVisibility();
                  }}
                  key={column.id}
                >
                  {column.header}
                </Checkbox>
              );
            })}
          </Flex>
        </MenuOptionGroup>
      </MenuList>
    </Menu>
  );
};
