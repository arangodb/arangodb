import React from "react";
import { getRouteForDB } from "../../../utils/arangoClient";
import { GraphDataType } from "./GraphData.types";
import {
  DEFAULT_URL_PARAMETERS,
  UrlParametersType
} from "./UrlParametersContext";
import { fetchUserConfig } from "./useFetchAndSetupGraphParams";
export const putUserConfig = async ({
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
      // we never save the 'mode' currently
      // because in the UI it's a temporary state
      mode: ""
    }
  };
  const data = await getRouteForDB(window.frontendConfig.db, "_api").put(
    `/user/${username}/config/graphs-v2`,
    { value: finalConfig }
  );
  return data.parsedBody;
};
export const useGraphSettingsHandlers = ({
  urlParams,
  graphData,
  graphName,
  setParams,
  hasDrawnOnce,
  setUrlParams
}: {
  setUrlParams: (urlParams: UrlParametersType) => void;
  urlParams: UrlParametersType;
  graphData?: GraphDataType;
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
    /**
     * - If nodeStart is "undefined", we want to remove it.
     * - If it is an empty string (""), we set it from settings.startVertex.
     *   - This avoids random vertex on clicking 'Apply'
     */
    let { nodeStart } = newParams;
    if (nodeStart === "") {
      nodeStart = graphData?.settings.startVertex?._id || nodeStart;
    }
    const fullConfig = await fetchUserConfig();
    await putUserConfig({ params: newParams, fullConfig, graphName });
    const finalParams = { ...newParams, nodeStart };
    setParams(finalParams);
    hasDrawnOnce.current = false;
  };

  const onRestoreDefaults = async () => {
    setUrlParams(DEFAULT_URL_PARAMETERS);
    await onApplySettings(DEFAULT_URL_PARAMETERS);
  };

  return { onApplySettings, onRestoreDefaults };
};
