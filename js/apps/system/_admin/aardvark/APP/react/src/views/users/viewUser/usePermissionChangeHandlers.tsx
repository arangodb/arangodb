import { CellContext } from "@tanstack/react-table";
import { useState } from "react";
import { getCurrentDB } from "../../../utils/arangoClient";
import { encodeHelper } from "../../../utils/encodeHelper";
import { notifyError } from "../../../utils/notifications";
import {
  CollectionType,
  DatabaseTableType
} from "./CollectionsPermissionsTable";
import { useUsername } from "./useFetchDatabasePermissions";

export type SystemDatabaseActionState = {
  status: "open" | "dismissed" | "confirmed";
  databaseName: string;
  permission: string;
};
export const usePermissionChangeHandlers = ({
  refetchDatabasePermissions
}: {
  refetchDatabasePermissions?: () => void;
}) => {
  const { username } = useUsername();
  const [systemDatabaseActionState, setSystemDatabaseActionState] =
    useState<SystemDatabaseActionState>();

  const handleDatabasePermissionChange = async ({
    databaseName,
    permission
  }: {
    permission: string;
    databaseName: string;
  }) => {
    try {
      const { encoded: encodedDatabaseName } = encodeHelper(databaseName);
      const currentDbRoute = getCurrentDB().route(
        `_api/user/${username}/database/${encodedDatabaseName}`
      );
      if (permission === "undefined") {
        await currentDbRoute.delete();
        refetchDatabasePermissions?.();
        return;
      }
      await currentDbRoute.put({
        grant: permission
      });
      refetchDatabasePermissions?.();
    } catch (e: any) {
      notifyError(e.message);
    }
  };
  const handleDatabaseCellClick = async ({
    info,
    permission
  }: {
    info: CellContext<DatabaseTableType, unknown>;
    permission: string;
  }) => {
    const { databaseName } = info.row.original;
    if (
      databaseName === "_system" &&
      systemDatabaseActionState?.status !== "confirmed"
    ) {
      // show the modal if trying to change the _system database
      setSystemDatabaseActionState({
        status: "open",
        databaseName,
        permission
      });
      return;
    }
    await handleDatabasePermissionChange({
      permission,
      databaseName
    });
  };

  const handleCollectionCellClick = async ({
    permission,
    info,
    databaseName
  }: {
    permission: string;
    info: CellContext<CollectionType, unknown>;
    databaseName: string;
  }) => {
    const { collectionName } = info.row.original;
    try {
      const { encoded: encodedDatabaseName } = encodeHelper(databaseName);
      const currentDbRoute = getCurrentDB().route(
        `_api/user/${username}/database/${encodedDatabaseName}/${collectionName}`
      );
      if (permission === "undefined") {
        await currentDbRoute.delete();
        refetchDatabasePermissions?.();
        return;
      }
      await currentDbRoute.put({ grant: permission });
      refetchDatabasePermissions?.();
    } catch (e: any) {
      notifyError(e.message);
    }
  };
  const onConfirmSystemDatabasePermissionChange = async () => {
    const { databaseName, permission } = systemDatabaseActionState || {};
    if (!databaseName || !permission) {
      return;
    }
    setSystemDatabaseActionState({
      status: "confirmed",
      databaseName,
      permission
    });
    await handleDatabasePermissionChange({
      databaseName,
      permission
    });
    setSystemDatabaseActionState(undefined);
  };
  const onCancelSystemDatabasePermissionChange = () => {
    setSystemDatabaseActionState(undefined);
  };
  return {
    handleDatabaseCellClick,
    handleCollectionCellClick,
    onConfirmSystemDatabasePermissionChange,
    onCancelSystemDatabasePermissionChange,
    systemDatabaseActionState
  };
};
