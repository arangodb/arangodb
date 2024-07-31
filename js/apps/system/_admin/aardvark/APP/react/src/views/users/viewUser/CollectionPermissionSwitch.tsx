import { Flex, Radio, Spinner, Tag } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import React from "react";
import { DatabaseTableType } from "./CollectionsPermissionsTable";
import { getIsDefaultRow } from "./DatabasePermissionSwitch";
import { useUserPermissionsContext } from "./UserPermissionsContext";

/**
 * Returns true if the current cell is evaluated as the default,
 * based on the "max level" login
 * @returns
 */
const getIsCellDefaultForCollection = ({
  info,
  databaseTable,
  databaseName
}: {
  info: CellContext<any, unknown>;
  databaseTable: DatabaseTableType[];
  databaseName: string;
}) => {
  const isDefaultRow = getIsDefaultRow(info);
  const maxLevel =
    info.row.original.collectionName &&
    getMaxLevel({ databaseTable, databaseName });

  if (
    // if it's a collection we are in
    info.row.original.collectionName &&
    // only if it's set to 'default'
    info.row.original.permission === "undefined" &&
    // for all columns which match the max level
    info.column.id === maxLevel &&
    // it's not the collection level default row
    !isDefaultRow
  ) {
    return true;
  }
  return false;
};
export const CollectionPermissionSwitch = ({
  checked,
  info
}: {
  checked: boolean;
  info: CellContext<any, unknown>;
}) => {
  const { databaseName, isManagedUser, isRootUser } =
    (info.table.options.meta as any) ?? {};
  const isSystemDatabase = databaseName === "_system";
  const { handleCollectionCellClick } = useUserPermissionsContext();
  const { databaseTable } = useUserPermissionsContext();
  const isDefaultRow = getIsDefaultRow(info);
  const isUndefined = info.column.id === "undefined";
  const [isLoading, setIsLoading] = React.useState(false);

  if (isDefaultRow && isUndefined) {
    return null;
  }

  const isCellDefaultForCollection = getIsCellDefaultForCollection({
    info,
    databaseTable,
    databaseName
  });

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
          isLoading ||
          (isDefaultRow && isSystemDatabase && isRootUser) ||
          isManagedUser
        }
        isChecked={checked}
        onChange={handleChange}
        name={info.row.original.collectionName}
        _focus={{
          boxShadow: "none"
        }}
      />
      {isLoading && <Spinner size="sm" />}
      {isCellDefaultForCollection && <Tag size="sm">Default</Tag>}
    </Flex>
  );
};

/**
 * Note: ported over from the existing code as-is.
 * This returns the max level based on the documented backend logic.
 */
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
