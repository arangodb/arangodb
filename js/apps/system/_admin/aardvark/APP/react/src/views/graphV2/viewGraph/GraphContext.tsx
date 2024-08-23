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

type DatasetsType = {
  nodes: DataSet<NodeDataType>;
  edges: DataSet<EdgeDataType>;
};
type GraphContextType = {
  graphData?: GraphDataType;
  graphName: string;
  onApplySettings: (updatedParams?: { [key: string]: string }) => void;
  onRestoreDefaults: () => void;
  network?: Network;
  isGraphLoading?: boolean;
  fetchDuration?: number;
  datasets?: DatasetsType;
  setDatasets: (datasets: DatasetsType) => void;
  rightClickedEntity?: RightClickedEntityType;
  setRightClickedEntity: (
    rightClickedEntity: RightClickedEntityType | undefined
  ) => void;
  selectedAction?: SelectedActionType;
  setSelectedAction: React.Dispatch<
    React.SetStateAction<SelectedActionType | undefined>
  >;
  setNetwork: (network: Network) => void;
  onClearAction: () => void;
  selectedEntity?: SelectedEntityType;
  onSelectEntity: (data: SelectedEntityType) => void;
  hasDrawnOnce: MutableRefObject<boolean>;
  graphError?: {
    code: number;
    errorMessage?: string;
    response?: { parsedBody: { errorMessage: string } };
  };
};

const GraphContext = createContext<GraphContextType>({
  graphName: ""
} as GraphContextType);

const useGraphName = () => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);
  return graphName;
};

const useGraphActions = () => {
  const [selectedAction, setSelectedAction] = useState<
    SelectedActionType | undefined
  >();
  const [selectedEntity, onSelectEntity] = useState<SelectedEntityType>();
  const onClearAction = () => {
    setSelectedAction(undefined);
  };
  return {
    selectedEntity,
    selectedAction,
    onSelectEntity,
    setSelectedAction,
    onClearAction
  };
};

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const [network, setNetwork] = useState<Network>();
  const hasDrawnOnce = useRef(false);
  const [datasets, setDatasets] = useState<DatasetsType>();
  const [rightClickedEntity, setRightClickedEntity] =
    useState<RightClickedEntityType>();

  const graphName = useGraphName();

  const { params, urlParams, setUrlParams, setParams } =
    useFetchAndSetupGraphParams({
      graphName
    });

  const { graphData, isGraphLoading, fetchDuration, graphError } =
    useFetchGraphData({ graphName, params });

  const { onApplySettings, onRestoreDefaults } = useGraphSettingsHandlers({
    urlParams,
    graphData,
    graphName,
    setParams,
    hasDrawnOnce,
    setUrlParams
  });

  const {
    selectedEntity,
    selectedAction,
    onSelectEntity,
    setSelectedAction,
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
        selectedAction,
        setSelectedAction,
        onClearAction,
        isGraphLoading,
        rightClickedEntity,
        setRightClickedEntity,
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
