import { useState } from "react";
import useSWR from "swr";
import { getRouteForDB } from "../../utils/arangoClient";
import { UrlParametersType } from "./UrlParametersContext";
import { GraphDataType } from "./GraphData.types";

export const fetchGraphData = async ({
  graphName,
  params
}: {
  graphName: string;
  params: UrlParametersType | undefined;
}) => {
  if (!params) {
    return Promise.resolve();
  }
  const data = await getRouteForDB(window.frontendConfig.db, "_admin").get(
    `/aardvark/visgraph/${graphName}`,
    params
  );
  return data.body;
};

export const useFetchGraphData = ({
  graphName,
  params
}: {
  graphName: string;
  params: UrlParametersType | undefined;
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
  return { data, error, isLoading, fetchDuration };
};
