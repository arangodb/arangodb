import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";

export const useFetchGraphs = () => {
  const { data } = useSWR("/graph", async () => {
    const db = getCurrentDB();
    const data = await db.listGraphs();
    return data;
  });
  return { graphs: data };
};
