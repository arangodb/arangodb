import { Flex, Radio, Spinner, Tag } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import React from "react";
import { DatabaseTableType } from "./CollectionsPermissionsTable";
import { getIsDefaultRow } from "./DatabasePermissionSwitch";
import { useUserPermissionsContext } from "./UserPermissionsContext";

export const CollectionPermissionSwitch = ({
  checked,
  info
}: {
  checked: boolean;
  info: CellContext<any, unknown>;
}) => {
  const { databaseName, username } = (info.table.options.meta || {}) as any;
  const isSystemDatabase = databaseName === "_system";
  const isRootUser = username === "root";
  const { handleCollectionCellClick } = useUserPermissionsContext();
  const { databaseTable } = useUserPermissionsContext();
  const maxLevel =
    info.row.original.collectionName &&
    getMaxLevel({ databaseTable, databaseName });
  const isDefaultRow = getIsDefaultRow(info);
  const isUndefined = info.column.id === "undefined";
  const [isLoading, setIsLoading] = React.useState(false);

  if (isDefaultRow && isUndefined) {
    return null;
  }
  let isDefaultForCollection = false;
  if (
    // if it's a collection we are in
    info.row.original.collectionName &&
    // only if it's set to 'default'
    info.row.original.permission === "undefined" &&
    // for all columns which match the max level
    info.column.id === maxLevel &&
    // it's not he collection level default row
    !isDefaultRow
  ) {
    isDefaultForCollection = true;
  }

  const handleChange = async () => {
    setIsLoading(true);
    await handleCollectionCellClick({
      info,
      permission: info.column.id,
      databaseName
    });
    setIsLoading(false);
  };
  return (
    <Flex gap="3">
      <Radio
        isDisabled={
          isLoading || (isDefaultRow && isSystemDatabase && isRootUser)
        }
        isChecked={checked}
        onChange={handleChange}
      />
      {isLoading && <Spinner size="sm" />}
      {isDefaultForCollection && <Tag size="sm">Default</Tag>}
    </Flex>
  );
};
const getMaxLevel = ({
  databaseTable,
  databaseName
}: {
  databaseTable: DatabaseTableType[];
  databaseName: string;
}) => {
  const serverLevelDefaultPermission = databaseTable.find(
    (row: any) => row.databaseName === "*"
  )?.permission;
  const databaseLevelDefaultPermission = databaseTable.find(
    (row: any) => row.databaseName === databaseName
  )?.permission;
  const collectionLevelDefaultPermission = databaseTable
    .find((row: any) => row.databaseName === databaseName)
    ?.collections.find((row: any) => row.collectionName === "*")?.permission;

  if (
    databaseLevelDefaultPermission === "undefined" &&
    collectionLevelDefaultPermission === "undefined" &&
    serverLevelDefaultPermission !== "undefined"
  ) {
    return serverLevelDefaultPermission;
  } else if (
    collectionLevelDefaultPermission === "undefined" ||
    collectionLevelDefaultPermission === "none"
  ) {
    // means - use default is selected here
    if (
      serverLevelDefaultPermission === "rw" ||
      databaseLevelDefaultPermission === "rw"
    ) {
      return "rw";
    } else if (
      serverLevelDefaultPermission === "ro" ||
      databaseLevelDefaultPermission === "ro"
    ) {
      return "ro";
    } else if (
      serverLevelDefaultPermission === "none" ||
      databaseLevelDefaultPermission === "none"
    ) {
      return "none";
    } else {
      return "undefined";
    }
  } else if (
    serverLevelDefaultPermission === "rw" ||
    databaseLevelDefaultPermission === "rw" ||
    collectionLevelDefaultPermission === "rw"
  ) {
    return "rw";
  } else if (
    serverLevelDefaultPermission === "ro" ||
    databaseLevelDefaultPermission === "ro" ||
    collectionLevelDefaultPermission === "ro"
  ) {
    return "ro";
  } else if (
    databaseLevelDefaultPermission === "none" ||
    collectionLevelDefaultPermission === "none"
  ) {
    if (serverLevelDefaultPermission !== "undefined") {
      return serverLevelDefaultPermission;
    }
    return "none";
  } else {
    return "undefined";
  }
};
