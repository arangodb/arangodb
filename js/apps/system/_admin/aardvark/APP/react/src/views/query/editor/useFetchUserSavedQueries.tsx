import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
import { getQueryStorageKey } from "../queryHelper";
export type QueryType = {
  name: string;
  value: string;
  modified_at?: number;
  created_at?: number;
  parameter: any;
  isTemplate?: boolean;
};

export const useFetchUserSavedQueries = () => {
  const storageKey = getQueryStorageKey();
  const fetchUser = async () => {
    // if LDAP is enabled, calls to /_api/user will fail
    // so we need to fetch from localStorage
    const currentUser = window.App.currentUser || "root";
    if (window.frontendConfig.ldapEnabled) {
      const savedQueries = JSON.parse(
        localStorage.getItem(storageKey) || "[]"
      ) as QueryType[];
      return savedQueries.reverse();
    }
    const currentDB = getCurrentDB();
    const user = await currentDB.getUser(currentUser);
    const savedQueries = user.extra?.queries;
    return savedQueries.reverse();
  };
  const { data, isLoading } = useSWR<QueryType[]>("/savedQueries", fetchUser);
  return { savedQueries: data, isLoading };
};
