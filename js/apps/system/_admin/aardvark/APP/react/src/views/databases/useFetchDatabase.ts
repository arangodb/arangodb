import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient"

export const useFetchDatabase = (name: string) => {
  const { data } = useSWR(`/databases/${name}`, async () => {
    const db = getCurrentDB();
    const {sharding, ...data} = await db.database(name).get();
    return {
      ...data,
      // arangojs currently incorrectly types this as "satellite"
      // but the API always returns a number
      replicationFactor: (data.replicationFactor || 1) as number,
      writeConcern: data.writeConcern || 1,
      isOneShard: sharding === "single",
    }
  });
  return { database: data };
};
