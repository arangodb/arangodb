import { ArangoUser } from "arangojs/database";
import useSWR from "swr";
import { getCurrentDB } from "../../../utils/arangoClient";
export type QueryType = {
  name: string;
  value: string;
  modified_at: number;
  parameter: any;
};
export const useFetchUserSavedQueries = () => {
  const fetchUser = async () => {
    const currentDB = getCurrentDB();
    const path = `/_api/user/${encodeURIComponent(window.App.currentUser)}`;
    const user = await currentDB.request({
      absolutePath: false,
      path
    });
    const savedQueries = (user.body as ArangoUser).extra.queries;
    return savedQueries;
  };
  const { data, isLoading } = useSWR<QueryType[]>("/savedQueries", fetchUser);
  return { savedQueries: data, isLoading };
};
