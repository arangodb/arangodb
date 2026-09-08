import { useQuery } from "@tanstack/react-query";
import { getRouteForCurrentDB } from "../../../utils/arangoClient";
import { useInferredRbacMode } from "../../../utils/usePermissions";

export type PermissionType = "rw" | "ro" | "none" | "undefined";
export const useUsername = () => {
  const username = window.location.hash.split("#user/")[1].split("/")[0];
  return { username };
};
type FullDatabasePermissionsType = {
  [databaseName: string]: {
    permission: PermissionType;
    collections?: {
      [collectionName: string]: PermissionType;
    };
  };
};

export const useFetchDatabasePermissions = () => {
  const { username } = useUsername();
  const inferredRbacMode = useInferredRbacMode();
  const { data, refetch } = useQuery({
    queryKey: [username, "permissions"],
    // Classic grants have no data source in RBAC mode.
    enabled: !inferredRbacMode,
    queryFn: async () => {
      const url = `/_api/user/${username}/database`;
      const route = getRouteForCurrentDB(url);
      const data = await route.get({ full: "true" });
      return data.parsedBody.result as FullDatabasePermissionsType;
    }
  });
  return { databasePermissions: data, refetchDatabasePermissions: refetch };
};
