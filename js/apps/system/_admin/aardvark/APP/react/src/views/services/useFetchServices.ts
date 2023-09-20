import useSWR from "swr";
import { getAardvarkRouteForCurrentDb } from "../../utils/arangoClient";
import { ServiceDescription } from "./Service.types";

export const useFetchServices = () => {
  const { data } = useSWR("/services", async () => {
    const route = getAardvarkRouteForCurrentDb("foxxes");
    const data = await route.get();
    // fetch conf & deps for each service
    const services = data.body.map(async (service: ServiceDescription) => {
      const config = await route.get("config", {
        mount: service.mount
      });
      const deps = await route.get("deps", {
        mount: service.mount
      });
      return {
        ...service,
        config: config.body,
        deps: deps.body
      };
    });
    const finalServices = await Promise.all(services);
    return finalServices as ServiceDescription[];
  });
  return { services: data };
};
