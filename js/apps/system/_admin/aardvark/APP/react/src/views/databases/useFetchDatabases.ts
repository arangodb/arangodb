import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";
import { DatabaseDescription } from "./Database.types";

export const useFetchDatabases = () => {
  const { data } = useSWR("/databases", async () => {
    const db = getCurrentDB();
    const data = await db.listDatabases();
    return data.map(name => ({ name } as DatabaseDescription));
  });
  return { databases: data };
};
