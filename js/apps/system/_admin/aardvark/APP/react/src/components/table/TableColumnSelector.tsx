import {
  Badge,
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

export const TableColumnSelector = <Data extends object>({
  table,
  columns
}: {
  table: TableType<Data>;
  columns: ColumnDef<Data, any>[];
}) => {
  const isAnyColumnInvisible = columns.some(column => {
    const isVisible = table.getColumn(column.id || "")?.getIsVisible();
    return !isVisible;
  });

  return (
    <Menu closeOnSelect={false}>
      <MenuButton
        as={Button}
        borderColor="gray.900"
        variant="outline"
        size="sm"
      >
        Columns
        {isAnyColumnInvisible && <ColumnInvisibleIndicator />}
      </MenuButton>
      <MenuList minWidth="200px">
        <MenuOptionGroup defaultChecked type="checkbox">
          <Flex paddingX="4" gap="2" direction="column">
            {columns.map(column => {
              const tableColumn = column.id
                ? table.getColumn(column.id)
                : undefined;
              const isVisible = tableColumn?.getIsVisible();
              const header = table
                .getFlatHeaders()
                .find(header => header.id === column.id);
              if (!header || !tableColumn) {
                return null;
              }
              return (
                <Checkbox
                  isChecked={isVisible}
                  onChange={() => {
                    tableColumn?.toggleVisibility();
                  }}
                  key={column.id}
                >
                  {typeof column.header === "function"
                    ? column.header({ column: tableColumn, header, table })
                    : column.header}
                </Checkbox>
              );
            })}
          </Flex>
        </MenuOptionGroup>
        {isAnyColumnInvisible && (
          <Flex justifyContent="flex-end" paddingX="4">
            <Button
              size="xs"
              variant="ghost"
              textTransform="uppercase"
              colorScheme="blue"
              onClick={() => {
                table.toggleAllColumnsVisible(true);
              }}
            >
              Clear
            </Button>
          </Flex>
        )}
      </MenuList>
    </Menu>
  );
};

const ColumnInvisibleIndicator = () => {
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
    />
  );
};
