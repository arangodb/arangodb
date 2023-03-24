import { useDisclosure } from "@chakra-ui/react";
import { ArangojsResponse } from "arangojs/lib/request.node";
import React, { createContext, ReactNode, useContext, useState } from "react";
import useSWR from "swr";
import { Network } from "vis-network";
import { getRouteForDB } from "../../utils/arangoClient";
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";
import { VisGraphData } from "./VisGraphData.types";

type SelectedEntityType = {
  type: string;
  nodeId?: string;
  edgeId?: string;
};

export type SelectedActionType = {
  action: "delete" | "edit";
  entity: SelectedEntityType;
};

type GraphContextType = {
  graphData: VisGraphData | undefined;
  graphName: string;
  onApplySettings: () => void;
  network?: Network;
  isSettingsOpen?: boolean;
  isGraphLoading?: boolean;
  selectedEntity?: SelectedEntityType;
  setSelectedEntity: (selectedEntity: SelectedEntityType | undefined) => void;
  selectedAction?: SelectedActionType;
  setSelectedAction: (selectedAction: SelectedActionType | undefined) => void;
  setNetwork: (network: Network) => void;
  toggleSettings: () => void;
  onCloseSettings: () => void;
  onClearAction: () => void;
};

const GraphContext = createContext<GraphContextType>({
  graphName: ""
} as GraphContextType);

const fetchVisData = ({
  graphName,
  params
}: {
  graphName: string;
  params: typeof URLPARAMETERS;
}) => {
  return getRouteForDB(window.frontendConfig.db, "_admin")
    .get(`/aardvark/visgraph/${graphName}`, params)
    .then(data => {
      return data;
    });
};

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);
  let [network, setNetwork] = useState<Network>();
  let [selectedEntity, setSelectedEntity] = useState<SelectedEntityType>();

  const [urlParameters, setUrlParameters] = useState(URLPARAMETERS);
  const [params, setParams] = useState(URLPARAMETERS);
  const { data, isLoading: isGraphLoading } = useSWR<ArangojsResponse>(
    ["visData", graphName, params],
    () => fetchVisData({ graphName, params }),
    {
      keepPreviousData: true,
      revalidateIfStale: true
    }
  );
  const onApplySettings = () => {
    setParams(urlParameters);
  };
  const {
    onOpen: onOpenSettings,
    onClose: onCloseSettings,
    isOpen: isSettingsOpen
  } = useDisclosure();
  const toggleSettings = () => {
    isSettingsOpen ? onCloseSettings() : onOpenSettings();
  };
  const [selectedAction, setSelectedAction] = useState<
    SelectedActionType | undefined
  >();
  const onClearAction = () => {
    setSelectedAction(undefined);
  };
  return (
    <GraphContext.Provider
      value={{
        network,
        onApplySettings,
        setNetwork,
        graphData: data && (data.body as VisGraphData),
        graphName,
        toggleSettings,
        onCloseSettings,
        selectedAction,
        setSelectedAction,
        onClearAction,
        isSettingsOpen,
        isGraphLoading,
        selectedEntity,
        setSelectedEntity
      }}
    >
      <UrlParametersContext.Provider
        value={[urlParameters, setUrlParameters] as any}
      >
        {children}
      </UrlParametersContext.Provider>
    </GraphContext.Provider>
  );
};

export const useGraph = () => {
  return useContext(GraphContext);
};
