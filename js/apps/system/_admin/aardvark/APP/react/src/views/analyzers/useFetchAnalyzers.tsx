import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";

export const useFetchAnalyzers = () => {
  const { data } = useSWR("/analyzers", async () => {
    const db = getCurrentDB();
    const data = await db.listAnalyzers();
    return data;
  });
  return { analyzers: data };
};
