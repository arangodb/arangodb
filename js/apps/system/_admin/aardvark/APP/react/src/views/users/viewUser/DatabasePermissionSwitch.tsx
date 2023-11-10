import { Flex, Radio, Tag } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import React from "react";
import { useUserPermissionsContext } from "./UserPermissionsContext";

export const getIsDefaultRow = (info: CellContext<any, unknown>) => {
  return (
    info.row.original.databaseName === "*" ||
    info.row.original.collectionName === "*"
  );
};
const getIsDefaultForDatabase = (info: CellContext<any, unknown>) => {
  const useDefault = info.row.original.permission === "undefined";
  const rows = info.table.getRowModel().rows;
  const defaultValue = rows.find(row => row.original.databaseName === "*")
    ?.original.permission;
  const isDefault = useDefault && defaultValue === info.column.id;
  return isDefault;
};

export const DatabasePermissionSwitch = ({
  checked,
  info
}: {
  checked: boolean;
  info: CellContext<any, unknown>;
}) => {
  const isDefaultForDatabase = getIsDefaultForDatabase(info);
  const isDefaultRow = getIsDefaultRow(info);
  const isUndefined = info.column.id === "undefined";
  const { handleDatabaseCellClick } = useUserPermissionsContext();
  if (isDefaultRow && isUndefined) {
    return null;
  }
  const handleChange = () => {
    handleDatabaseCellClick({
      info,
      permission: info.column.id
    });
  };
  return (
    <Flex gap="3">
      <Radio isChecked={checked} onChange={handleChange} />
      {isDefaultForDatabase && <Tag size="sm">Default</Tag>}
    </Flex>
  );
};
