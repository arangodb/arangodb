import { useDisclosure } from "@chakra-ui/react";
import { ArangojsResponse } from "arangojs/lib/request.node";
import React, { createContext, ReactNode, useContext, useState } from "react";
import useSWRImmutable from "swr/immutable";
import { Network } from "vis-network";
import { getRouteForDB } from "../../utils/arangoClient";
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";
import { VisGraphData } from "./VisGraphData.types";

type GraphContextType = {
  graphData: VisGraphData | undefined;
  graphName: string;
  onApplySettings: () => void;
  network?: Network;
  isSettingsOpen?: boolean;
  isGraphLoading?: boolean;
  deleteEdgeModalData?: { edgeId: string };
  setNetwork: (network: Network) => void;
  toggleSettings: () => void;
  onCloseSettings: () => void;
  onDeleteEdge: (edgeId?: string) => void;
  onCancelDelete: () => void;
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

  const [urlParameters, setUrlParameters] = useState(URLPARAMETERS);
  const [params, setParams] = useState(URLPARAMETERS);
  const { data, isLoading: isGraphLoading } = useSWRImmutable<ArangojsResponse>(
    ["visData", graphName, params],
    () => fetchVisData({ graphName, params }),
    {
      keepPreviousData: true
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
  const [deleteEdgeModalData, setDeleteEdgeModalData] = useState<
    { edgeId: string } | undefined
  >();
  const onDeleteEdge = (edgeId?: string) => {
    console.log("ondelete", edgeId);
    if (!edgeId) {
      return;
    }
    setDeleteEdgeModalData({ edgeId });
  };
  const onCancelDelete = () => {
    setDeleteEdgeModalData(undefined);
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
        onDeleteEdge,
        onCancelDelete,
        deleteEdgeModalData,
        isSettingsOpen,
        isGraphLoading
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
