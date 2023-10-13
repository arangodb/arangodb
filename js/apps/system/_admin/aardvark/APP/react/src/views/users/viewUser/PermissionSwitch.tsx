import { Flex, Switch, Tag } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import React from "react";

export const getIsDefaultRow = (
  info: CellContext<any, unknown>
) => {
  return (
    info.row.original.databaseName === "*" ||
    info.row.original.collectionName === "*"
  );
};
const getIsDefaultForDatabase = (info: CellContext<any, unknown>) => {
  const useDefault = info.row.original.permission === "undefined";
  const rows = info.table.getRowModel().rows;
  const defaultValue = rows.find(
    row =>
      row.original.databaseName === "*" || row.original.collectionName === "*"
  )?.original.permission;
  const isDefault = useDefault && defaultValue === info.column.id;
  return isDefault;
};

export const PermissionSwitch = ({
  checked,
  info
}: {
  checked: boolean;
  info: CellContext<any, unknown>;
}) => {
  const isDefaultForDatabase = getIsDefaultForDatabase(info);
  const isDefaultRow = getIsDefaultRow(info);
  const isUndefined = info.column.id === "undefined";
  if (isDefaultRow && isUndefined) {
    return null;
  }
  return (
    <Flex gap="3">
      <Switch isChecked={checked} />
      {isDefaultForDatabase && <Tag size="sm">Default</Tag>}
    </Flex>
  );
};
