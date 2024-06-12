import { useEffect, useState } from "react";
import { getRouteForDB } from "../../../utils/arangoClient";
import {
  DEFAULT_URL_PARAMETERS,
  UrlParametersType
} from "./UrlParametersContext";

export const fetchUserConfig = async () => {
  const username = window.App.currentUser || "root";

  const data = await getRouteForDB(window.frontendConfig.db, "_api").get(
    `/user/${username}/config`
  );
  return data.parsedBody.result?.["graphs-v2"];
};

export const useFetchAndSetupGraphParams = ({
  graphName
}: {
  graphName: string;
}) => {
  const [urlParams, setUrlParams] = useState<UrlParametersType>(
    DEFAULT_URL_PARAMETERS
  );
  const [params, setParams] = useState<UrlParametersType>();
  useEffect(() => {
    async function fetchData() {
      const config = await fetchUserConfig();
      const paramKey = `${window.frontendConfig.db}_${graphName}`;
      const graphParams = config?.[paramKey] || DEFAULT_URL_PARAMETERS;
      setUrlParams(graphParams);
      setParams(graphParams);
    }
    fetchData();
  }, [graphName]);
  return { urlParams, setUrlParams, params, setParams };
};
