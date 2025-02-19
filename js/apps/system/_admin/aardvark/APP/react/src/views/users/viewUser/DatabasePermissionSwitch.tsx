import { Flex, Radio, Spinner, Tag } from "@chakra-ui/react";
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
  const { isManagedUser } = (info.table.options.meta || {}) as any;
  const isDefaultForDatabase = getIsDefaultForDatabase(info);
  const isDefaultRow = getIsDefaultRow(info);
  const isUndefined = info.column.id === "undefined";
  const { handleDatabaseCellClick } = useUserPermissionsContext();
  const [isLoading, setIsLoading] = React.useState(false);
  if (isDefaultRow && isUndefined) {
    return null;
  }
  const handleChange = async () => {
    setIsLoading(true);
    await handleDatabaseCellClick({
      info,
      permission: info.column.id
    });
    setIsLoading(false);
  };
  return (
    <Flex gap="3">
      <Radio
        isDisabled={isManagedUser}
        isChecked={checked}
        onChange={handleChange}
      />
      {isLoading && <Spinner size="sm" />}
      {isDefaultForDatabase && <Tag size="sm">Default</Tag>}
    </Flex>
  );
};
