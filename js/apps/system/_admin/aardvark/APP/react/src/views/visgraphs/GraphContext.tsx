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
import { UrlParametersContext } from "./UrlParametersContext";
import { useFetchAndSetupGraphParams } from "./useFetchAndSetupGraphParams";
import { useFetchGraphData } from "./useFetchGraphData";
import { useGraphSettingsHandlers } from "./useGraphSettingsHandlers";

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

const useGraphSettingsDisclosure = () => {
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

const useGraphActions = () => {
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
  return {
    selectedEntity,
    selectedAction,
    onSelectEntity,
    setSelectedAction,
    onAddEdge,
    onClearAction
  };
};

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const [network, setNetwork] = useState<Network>();
  const hasDrawnOnce = useRef(false);
  const [datasets, setDatasets] = useState<DatasetsType>();
  const [rightClickedEntity, setRightClickedEntity] = useState<
    RightClickedEntityType
  >();
  const graphName = useGraphName();

  const {
    params,
    urlParams,
    setUrlParams,
    setParams
  } = useFetchAndSetupGraphParams({
    graphName
  });

  const {
    graphData,
    isGraphLoading,
    fetchDuration,
    graphError
  } = useFetchGraphData({ graphName, params });

  const { onApplySettings, onRestoreDefaults } = useGraphSettingsHandlers({
    urlParams,
    graphData,
    graphName,
    setParams,
    hasDrawnOnce,
    setUrlParams
  });

  const { toggleSettings, isSettingsOpen } = useGraphSettingsDisclosure();

  const {
    selectedEntity,
    selectedAction,
    onSelectEntity,
    setSelectedAction,
    onAddEdge,
    onClearAction
  } = useGraphActions();

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
