import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import { DatabaseUserValues } from "./addUser/CreateUser.types";

export const useFetchUsers = () => {
  const { data } = useSWR("/users", async () => {
    const users = await getCurrentDB().listUsers();
    return users as DatabaseUserValues[];
  });
  return { users: data };
};
