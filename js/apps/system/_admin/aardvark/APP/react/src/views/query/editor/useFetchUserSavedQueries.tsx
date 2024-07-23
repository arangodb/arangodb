import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
export type QueryType = {
  name: string;
  value: string;
  modified_at?: number;
  created_at?: number;
  parameter: any;
  isTemplate?: boolean;
};

export const useFetchUserSavedQueries = () => {
  const fetchUser = async () => {
    // so we need to fetch from localStorage
    const currentUser = window.App.currentUser || "root";
    const currentDB = getCurrentDB();
    const user = await currentDB.getUser(currentUser);
    const savedQueries = user.extra?.queries;
    return savedQueries.reverse();
  };
  const { data, isLoading } = useSWR<QueryType[]>("/savedQueries", fetchUser);
  return { savedQueries: data, isLoading };
};
