import useSWR from "swr";
import { getCurrentDB, isOwnAccessLevelForbidden } from "./arangoClient";

const useOwnAccessLevel = () =>
  useSWR(
    `/user/${window.arangoHelper.getCurrentJwtUsername()}/database/${
      window.frontendConfig.db
    }`,
    () =>
      getCurrentDB().getUserAccessLevel(
        window.arangoHelper.getCurrentJwtUsername(),
        { database: window.frontendConfig.db }
      )
  );

// Inferred from the probe above; the server exposes no RBAC flag.
export const useInferredRbacMode = () =>
  isOwnAccessLevelForbidden(useOwnAccessLevel().error);

const usePermissions = () => {
  const { data, error } = useOwnAccessLevel();
  if (isOwnAccessLevelForbidden(error)) {
    return "rw";
  }
  return data || "none";
};

const userIsAdmin = (permission: string) =>
  permission === "rw" || !window.frontendConfig.authenticationEnabled;

export const useIsAdminUser = () => {
  const permission = usePermissions();
  return userIsAdmin(permission);
};
