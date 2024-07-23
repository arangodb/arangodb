import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { DatabaseUserValues } from "./addUser/CreateUser.types";

export const useFetchUsers = () => {
  const { data } = useSWR("/users", async () => {
    const route = getApiRouteForCurrentDB();
    const res = await route.get("user");
    return res.body.result as DatabaseUserValues[];
  });
  return { users: data };
};
