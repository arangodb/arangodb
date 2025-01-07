import useSWR from "swr";
import { getAdminRouteForCurrentDB } from "../../utils/arangoClient";

export const useFetchInitialValues = () => {
  const { data, error } = useSWR(`/databaseDefaults`, async () => {
    const route = getAdminRouteForCurrentDB();
    const res = await route.get(`server/databaseDefaults`);
    const { replicationFactor, sharding, writeConcern } = res.parsedBody;
    return {
      name: "",
      users: [],
      isOneShard: sharding === "single" || window.frontendConfig.forceOneShard,
      isSatellite: replicationFactor === "satellite",
      replicationFactor:
        replicationFactor === "satellite" ? 1 : replicationFactor,
      writeConcern
    };
  });

  if (error) {
    window.App.navigate("#databases", { trigger: true });
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${error.errorNum}: ${error.message}`
    );
  }

  return { defaults: data };
};
