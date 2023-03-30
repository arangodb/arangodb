import { useDisclosure } from "@chakra-ui/react";
import React, {
  createContext,
  MutableRefObject,
  ReactNode,
  useContext,
  useEffect,
  useRef,
  useState
} from "react";
import useSWR from "swr";
import { DataSet, Network } from "vis-network";
import { getRouteForDB } from "../../utils/arangoClient";
import {
  DEFAULT_URL_PARAMETERS,
  UrlParametersContext,
  UrlParametersType
} from "./UrlParametersContext";
import { EdgeDataType, NodeDataType, VisGraphData } from "./VisGraphData.types";

type ActionEntityType = {
  type: string;
  nodeId?: string;
  edgeId?: string;
};

export type SelectedActionType = {
  action: "delete" | "edit" | "add" | "loadFullGraph";
  entityType?: "node" | "edge";
  from?: string;
  to?: string;
  callback?: any;
  entity?: ActionEntityType;
};

export type SelectedEntityType = { entityId: string; type: "node" | "edge" };

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
  onRestoreDefaults: () => void;
  network?: Network;
  isSettingsOpen?: boolean;
  isGraphLoading?: boolean;
  fetchDuration?: number;
  datasets?: DatasetsType;
  setDatasets: (datasets: DatasetsType) => void;
  rightClickedEntity?: ActionEntityType;
  setRightClickedEntity: (
    rightClickedEntity: ActionEntityType | undefined
  ) => void;
  selectedAction?: SelectedActionType;
  setSelectedAction: (selectedAction: SelectedActionType | undefined) => void;
  setNetwork: (network: Network) => void;
  toggleSettings: () => void;
  onCloseSettings: () => void;
  onClearAction: () => void;
  onAddEdge: (args: AddEdgeArgs) => void;
  selectedEntity?: SelectedEntityType;
  onSelectEntity: (data: SelectedEntityType) => void;
  hasDrawnOnce: MutableRefObject<boolean>;
};

const GraphContext = createContext<GraphContextType>({
  graphName: ""
} as GraphContextType);

export const fetchVisData = async ({
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

export const fetchUserConfig = async () => {
  const username = window.App.currentUser || "root";

  const data = await getRouteForDB(window.frontendConfig.db, "_api").get(
    `/user/${username}/config`
  );
  return data.body.result?.visgraphs;
};

export const putUserConfig = async ({
  params,
  fullConfig,
  graphName
}: {
  params: any;
  fullConfig: any;
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

const useSetupParams = ({ graphName }: { graphName: string }) => {
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
export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);
  const [network, setNetwork] = useState<Network>();
  const hasDrawnOnce = useRef(false);
  const [datasets, setDatasets] = useState<DatasetsType>();
  const [fetchDuration, setFetchDuration] = useState<number>();
  const [rightClickedEntity, setRightClickedEntity] = useState<
    ActionEntityType
  >();
  const { setUrlParams, setParams, params, urlParams } = useSetupParams({
    graphName
  });
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
      revalidateIfStale: true,
      revalidateOnFocus: false
    }
  );

  const onRestoreDefaults = async () => {
    setUrlParams(DEFAULT_URL_PARAMETERS);
    await onApplySettings(DEFAULT_URL_PARAMETERS);
  };
  const onApplySettings = async (updatedParams?: {
    [key: string]: string | number | boolean;
  }) => {
    const newParams = {
      ...urlParams,
      ...(updatedParams ? updatedParams : {})
    } as UrlParametersType;
    let { nodeStart } = newParams;
    if (!nodeStart) {
      nodeStart = graphData?.settings.startVertex._id || nodeStart;
    }
    const fullConfig = await fetchUserConfig();
    await putUserConfig({ params: newParams, fullConfig, graphName });
    const finalParams = { ...newParams, nodeStart };
    setParams(finalParams);
    hasDrawnOnce.current = false;
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
  const [selectedEntity, onSelectEntity] = useState<SelectedEntityType>();

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
        fetchDuration,
        selectedEntity,
        onSelectEntity,
        onRestoreDefaults,
        hasDrawnOnce
      }}
    >
      <UrlParametersContext.Provider value={{ urlParams, setUrlParams }}>
        {children}
      </UrlParametersContext.Provider>
    </GraphContext.Provider>
  );
};

export const useGraph = () => {
  return useContext(GraphContext);
};
