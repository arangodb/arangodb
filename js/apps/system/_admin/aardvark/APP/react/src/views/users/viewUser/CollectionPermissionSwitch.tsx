import { Flex, Radio, Tag } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import React from "react";
import { getIsDefaultRow } from "./DatabasePermissionSwitch";

export const CollectionPermissionSwitch = ({
  checked,
  info
}: {
  checked: boolean;
  info: CellContext<any, unknown>;
}) => {
  const maxLevel = info.row.original.collectionName && getMaxLevel(info);
  const isDefaultRow = getIsDefaultRow(info);
  const isUndefined = info.column.id === "undefined";
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
  const handleChange = () => {
    const { handleCellClick } = info.table.options.meta as any;
    handleCellClick?.({
      info,
      permission: info.column.id
    });
  };
  return (
    <Flex gap="3">
      <Radio isChecked={checked} onChange={handleChange} />
      {isDefaultForCollection && <Tag size="sm">Default</Tag>}
    </Flex>
  );
};
const getMaxLevel = (info: CellContext<any, unknown>) => {
  const { databaseTable, databaseName } = (info.table.options.meta as any) || {
    databaseTable: []
  };
  const serverLevelDefaultPermission = databaseTable.find(
    (row: any) => row.databaseName === "*"
  )?.permission;
  const databaseLevelDefaultPermission = databaseTable.find(
    (row: any) => row.databaseName === databaseName
  )?.permission;
  const collectionLevelDefaultPermission = databaseTable
    .find((row: any) => row.databaseName === databaseName)
    ?.collections.find((row: any) => row.collectionName === "*")?.permission;

  // console.log({
  //   serverLevelDefaultPermission,
  //   databaseLevelDefaultPermission,
  //   collectionLevelDefaultPermission
  // });
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
