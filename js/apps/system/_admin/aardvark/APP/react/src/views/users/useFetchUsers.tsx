import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";

export const useFetchUsers = () => {
  const { data } = useSWR("/users", async () => {
    const db = getCurrentDB();
    const route = db.route("_api/user");
    const data = await route.get();
    return data.body.result;
  });
  return { users: data };
};
