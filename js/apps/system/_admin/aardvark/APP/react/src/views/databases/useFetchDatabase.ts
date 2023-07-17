import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";

export const useFetchDatabase = (name: string) => {
  const { data } = useSWR(
    `/databases/${encodeURIComponent(name)}`,
    async () => {
      const db = getCurrentDB();
      const { sharding, ...data } = await db.database(name).get();
      return {
        ...data,
        isOneShard: sharding === "single",
        isSatellite: data.replicationFactor === "satellite",
        replicationFactor:
          data.replicationFactor === "satellite" ? 1 : data.replicationFactor,
        writeConcern: data.writeConcern || 1
      };
    }
  );
  return { database: data };
};
