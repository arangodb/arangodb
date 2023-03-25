import { useDisclosure } from "@chakra-ui/react";
import React, { createContext, ReactNode, useContext, useState } from "react";
import useSWR from "swr";
import { DataSet, Network } from "vis-network";
import { getRouteForDB } from "../../utils/arangoClient";
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";
import { EdgeDataType, NodeDataType, VisGraphData } from "./VisGraphData.types";

type SelectedEntityType = {
  type: string;
  nodeId?: string;
  edgeId?: string;
};

export type SelectedActionType = {
  action: "delete" | "edit" | "add";
  entityType?: "node" | "edge";
  from?: string;
  to?: string;
  callback?: any;
  entity: SelectedEntityType;
};

type AddEdgeArgs = {
  data: { from: string; to: string };
  callback: any;
};

type DatasetsType = {
  nodes: DataSet<NodeDataType>;
  edges: DataSet<EdgeDataType>;
};
type GraphContextType = {
  graphData: VisGraphData | undefined;
  graphName: string;
  onApplySettings: (updatedParams?: { [key: string]: string }) => void;
  network?: Network;
  isSettingsOpen?: boolean;
  isGraphLoading?: boolean;
  loadFullGraph: boolean;
  fetchDuration?: number;
  setLoadFullGraph: (load: boolean) => void;
  datasets?: DatasetsType;
  setDatasets: (datasets: DatasetsType) => void;
  rightClickedEntity?: SelectedEntityType;
  setRightClickedEntity: (
    rightClickedEntity: SelectedEntityType | undefined
  ) => void;
  selectedAction?: SelectedActionType;
  setSelectedAction: (selectedAction: SelectedActionType | undefined) => void;
  setNetwork: (network: Network) => void;
  toggleSettings: () => void;
  onCloseSettings: () => void;
  onClearAction: () => void;
  onAddEdge: (args: AddEdgeArgs) => void;
  selectedEntity: string;
  onSelectEntity: (edgeId: string) => void;
};

const GraphContext = createContext<GraphContextType>({
  graphName: ""
} as GraphContextType);

export const fetchVisData = async ({
  graphName,
  params
}: {
  graphName: string;
  params: typeof URLPARAMETERS;
}) => {
  const data = await getRouteForDB(window.frontendConfig.db, "_admin").get(
    `/aardvark/visgraph/${graphName}`,
    params
  );
  return data.body;
};

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);
  let [loadFullGraph, setLoadFullGraph] = useState(false);
  let [network, setNetwork] = useState<Network>();
  const [datasets, setDatasets] = useState<DatasetsType>();
  const [fetchDuration, setFetchDuration] = useState<number>();
  let [rightClickedEntity, setRightClickedEntity] = useState<
    SelectedEntityType
  >();

  const [urlParameters, setUrlParameters] = useState(URLPARAMETERS);
  const [params, setParams] = useState(URLPARAMETERS);
  const { data: graphData, isLoading: isGraphLoading } = useSWR<VisGraphData>(
    ["visData", graphName, params],
    async () => {
      const fetchStarted = new Date();
      const data = await fetchVisData({ graphName, params });
      const fetchDuration = Math.abs(
        new Date().getTime() - fetchStarted.getTime()
      );
      setFetchDuration(fetchDuration);
      return data;
    },
    {
      keepPreviousData: true,
      revalidateIfStale: true
    }
  );

  const onApplySettings = (updatedParams?: { [key: string]: string }) => {
    const newParams = { ...urlParameters, ...updatedParams };
    let { nodeStart } = newParams;
    if (!nodeStart) {
      nodeStart = graphData?.settings.startVertex._id || nodeStart;
    }
    setParams({ ...newParams, nodeStart });
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
  const onAddEdge = ({ data, callback }: AddEdgeArgs) => {
    setSelectedAction(action => {
      if (!action) {
        return;
      }
      return { ...action, from: data.from, to: data.to, callback };
    });
  };
  const [selectedEntity, onSelectEntity] = useState("");

  return (
    <GraphContext.Provider
      value={{
        network,
        onApplySettings,
        setNetwork,
        graphData,
        graphName,
        toggleSettings,
        onCloseSettings,
        selectedAction,
        setSelectedAction,
        onClearAction,
        isSettingsOpen,
        isGraphLoading,
        rightClickedEntity,
        setRightClickedEntity,
        onAddEdge,
        datasets,
        setDatasets,
        setLoadFullGraph,
        loadFullGraph,
        fetchDuration,
        selectedEntity,
        onSelectEntity
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
