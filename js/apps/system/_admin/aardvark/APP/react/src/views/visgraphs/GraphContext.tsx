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
  RightClickedEntityType,
  SelectedActionType,
  SelectedEntityType
} from "./GraphAction.types";
import { EdgeDataType, GraphDataType, NodeDataType } from "./GraphData.types";
import {
  DEFAULT_URL_PARAMETERS,
  UrlParametersContext
} from "./UrlParametersContext";
import { useApplyGraphSettings } from "./useApplyGraphSettings";
import { useFetchGraphData } from "./useFetchGraphData";
import { useSetupGraphParams } from "./useSetupGraphParams";

type AddEdgeArgs = {
  data: { from: string; to: string };
  callback: (edge: EdgeDataType) => void;
};

type DatasetsType = {
  nodes: DataSet<NodeDataType>;
  edges: DataSet<EdgeDataType>;
};
type GraphContextType = {
  graphData: GraphDataType | undefined;
  graphName: string;
  onApplySettings: (updatedParams?: { [key: string]: string }) => void;
  onRestoreDefaults: () => void;
  network?: Network;
  isSettingsOpen?: boolean;
  isGraphLoading?: boolean;
  fetchDuration?: number;
  datasets?: DatasetsType;
  setDatasets: (datasets: DatasetsType) => void;
  rightClickedEntity?: RightClickedEntityType;
  setRightClickedEntity: (
    rightClickedEntity: RightClickedEntityType | undefined
  ) => void;
  selectedAction?: SelectedActionType;
  setSelectedAction: (selectedAction: SelectedActionType | undefined) => void;
  setNetwork: (network: Network) => void;
  toggleSettings: () => void;
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

const useGraphName = () => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);
  return graphName;
};

const useSettingsDisclosure = () => {
  const {
    onOpen: onOpenSettings,
    onClose: onCloseSettings,
    isOpen: isSettingsOpen
  } = useDisclosure();
  const toggleSettings = () => {
    isSettingsOpen ? onCloseSettings() : onOpenSettings();
  };
  return { toggleSettings, isSettingsOpen };
};

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const graphName = useGraphName();
  const [network, setNetwork] = useState<Network>();
  const hasDrawnOnce = useRef(false);
  const [datasets, setDatasets] = useState<DatasetsType>();
  const [rightClickedEntity, setRightClickedEntity] = useState<
    RightClickedEntityType
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

  const { toggleSettings, isSettingsOpen } = useSettingsDisclosure();
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
