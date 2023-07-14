import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { ArangoUser } from "arangojs/database";

export const useFetchUsers = () => {
  const { data } = useSWR("/users", async () => {
    const route = getApiRouteForCurrentDB();
    const res = await route.get("user");
    return res.body.result as ArangoUser[];
  });
  return { users: data };
};
