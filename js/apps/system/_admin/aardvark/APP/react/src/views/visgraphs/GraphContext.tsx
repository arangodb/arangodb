import { useDisclosure } from "@chakra-ui/react";
import React, {
  createContext,
  MutableRefObject,
  ReactNode,
  useContext,
  useRef,
  useState
} from "react";
import { DataSet, Network } from "vis-network";
import {
  DEFAULT_URL_PARAMETERS,
  UrlParametersContext
} from "./UrlParametersContext";
import { useApplyGraphSettings } from "./useApplyGraphSettings";
import { useFetchGraphData } from "./useFetchGraphData";
import { useSetupGraphParams } from "./useSetupGraphParams";
import {
  EdgeDataType,
  NodeDataType,
  VisGraphData,
  VisPointer
} from "./VisGraphData.types";

type ActionEntityType = {
  type: string;
  nodeId?: string;
  edgeId?: string;
  pointer: VisPointer;
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
  graphError?: { code: number; response: { body: { errorMessage: string } } };
};

const GraphContext = createContext<GraphContextType>({
  graphName: ""
} as GraphContextType);

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);
  const [network, setNetwork] = useState<Network>();
  const hasDrawnOnce = useRef(false);
  const [datasets, setDatasets] = useState<DatasetsType>();
  const [rightClickedEntity, setRightClickedEntity] = useState<
    ActionEntityType
  >();
  const { setUrlParams, setParams, params, urlParams } = useSetupGraphParams({
    graphName
  });

  const {
    data: graphData,
    isLoading: isGraphLoading,
    error: graphError,
    fetchDuration
  } = useFetchGraphData({ graphName, params });
  const { onApplySettings } = useApplyGraphSettings({
    urlParams,
    graphData,
    graphName,
    setParams,
    hasDrawnOnce
  });

  const onRestoreDefaults = async () => {
    setUrlParams(DEFAULT_URL_PARAMETERS);
    await onApplySettings(DEFAULT_URL_PARAMETERS);
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
        hasDrawnOnce,
        graphError
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
