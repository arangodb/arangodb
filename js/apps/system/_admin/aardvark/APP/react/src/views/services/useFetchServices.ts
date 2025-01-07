import useSWR from "swr";
import { getAardvarkRouteForCurrentDb } from "../../utils/arangoClient";
import { ServiceDescription } from "./Service.types";

export const useFetchServices = () => {
  const { data } = useSWR("/services", async () => {
    const route = getAardvarkRouteForCurrentDb("foxxes");
    const data = await route.get();
    // fetch confing & deps for each service
    const services = data.parsedBody.map(
      async (service: ServiceDescription) => {
        const config = await route.get("config", {
          mount: service.mount
        });
        const deps = await route.get("deps", {
          mount: service.mount
        });
        return {
          ...service,
          config: config.parsedBody,
          deps: deps.parsedBody
        };
      }
    );
    const finalServices = await Promise.all<ServiceDescription>(services);
    return finalServices;
  });
  return { services: data };
};
