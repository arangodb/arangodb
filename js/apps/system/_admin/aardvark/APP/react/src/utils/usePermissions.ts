import useSWR from "swr";
import { getCurrentDB, isRbacRejection } from "./arangoClient";

const usePermissions = () => {
  const { data, error } = useSWR(
    `/user/${window.arangoHelper.getCurrentJwtUsername()}/database/${
      window.frontendConfig.db
    }`,
    () =>
      getCurrentDB().getUserAccessLevel(
        window.arangoHelper.getCurrentJwtUsername(),
        { database: window.frontendConfig.db }
      )
  );
  if (isRbacRejection(error)) {
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
