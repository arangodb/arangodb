import useSWR from "swr";
import { getCurrentDB } from "../../utils/arangoClient";

const useCurrentDbProperties = () => {
  const currentDbName = window.frontendConfig.db;
  const { data } = useSWR(`/${currentDbName}/properties`, () => {
    const currentDb = getCurrentDB();
    return currentDb.request({ method: "GET", path: "/_api/database/current" });
  });
  return data?.body.result;
};
export const useIsOneShardDb = (): boolean => {
  const currentDbProperties = useCurrentDbProperties();
  return (
    currentDbProperties?.sharding === "single" ||
    window.frontendConfig.forceOneShard ||
    false
  );
};
