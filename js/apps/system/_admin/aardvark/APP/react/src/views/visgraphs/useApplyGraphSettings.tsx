import React from "react";
import { getRouteForDB } from "../../utils/arangoClient";
import { UrlParametersType } from "./UrlParametersContext";
import { fetchUserConfig } from "./useSetupGraphParams";
import { GraphDataType } from "./GraphData.types";
const putUserConfig = async ({
  params,
  fullConfig,
  graphName
}: {
  params: UrlParametersType;
  fullConfig: { [databaseName: string]: UrlParametersType };
  graphName: string;
}) => {
  const username = window.App.currentUser || "root";
  const paramKey = `${window.frontendConfig.db}_${graphName}`;

  const finalConfig = {
    ...fullConfig,
    [paramKey]: {
      ...params,
      mode: ""
    }
  };
  const data = await getRouteForDB(
    window.frontendConfig.db,
    "_api"
  ).put(`/user/${username}/config/visgraphs`, { value: finalConfig });
  return data.body;
};
export const useApplyGraphSettings = ({
  urlParams,
  graphData,
  graphName,
  setParams,
  hasDrawnOnce
}: {
  urlParams: UrlParametersType;
  graphData: GraphDataType | undefined;
  graphName: string;
  setParams: React.Dispatch<
    React.SetStateAction<UrlParametersType | undefined>
  >;
  hasDrawnOnce: React.MutableRefObject<boolean>;
}) => {
  const onApplySettings = async (updatedParams?: {
    [key: string]: string | number | boolean;
  }) => {
    const newParams = {
      ...urlParams,
      ...(updatedParams ? updatedParams : {})
    } as UrlParametersType;
    // if the new params nodeStart is an "empty string",
    // (not "undefined"), we set it from settings.startVertex.
    // This is when nodeStart is blank & we click apply
    let { nodeStart } = newParams;
    if (!nodeStart && nodeStart !== undefined) {
      nodeStart = graphData?.settings.startVertex?._id || nodeStart;
    }
    const fullConfig = await fetchUserConfig();
    await putUserConfig({ params: newParams, fullConfig, graphName });
    const finalParams = { ...newParams, nodeStart };
    setParams(finalParams);
    hasDrawnOnce.current = false;
  };
  return { onApplySettings };
};
