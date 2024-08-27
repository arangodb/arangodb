import { useState } from "react";
import useSWR from "swr";
import { getRouteForDB } from "../../../utils/arangoClient";
import { GraphDataType } from "./GraphData.types";
import { UrlParametersType } from "./UrlParametersContext";

export const fetchGraphData = async ({
  graphName,
  params
}: {
  graphName: string;
  params?: UrlParametersType;
}) => {
  if (!params) {
    return Promise.resolve();
  }
  const data = await getRouteForDB(window.frontendConfig.db, "_admin").get(
    `/aardvark/graphs-v2/${graphName}`,
    params
  );
  return data.parsedBody;
};

export const useFetchGraphData = ({
  graphName,
  params
}: {
  graphName: string;
  params?: UrlParametersType;
}) => {
  const [fetchDuration, setFetchDuration] = useState<number>();
  const { data, error, isLoading } = useSWR<GraphDataType>(
    ["visData", graphName, params],
    async () => {
      const fetchStarted = new Date();
      const data = await fetchGraphData({ graphName, params });
      const fetchDuration = Math.abs(
        new Date().getTime() - fetchStarted.getTime()
      );
      setFetchDuration(fetchDuration);
      return data;
    },
    {
      keepPreviousData: true,
      revalidateIfStale: true,
      revalidateOnFocus: false
    }
  );

  return {
    graphData: data,
    isGraphLoading: isLoading,
    graphError: error,
    fetchDuration
  };
};
