import React, { createContext, ReactNode, useContext } from "react";
import { DataSet, Network } from "vis-network";
import { EdgeDataType, NodeDataType } from "../../graphV2/GraphData.types";
import {
  getLayout,
  SettingsType,
  useSetupQueryGraph
} from "../queryResults/useSetupQueryGraph";

type QueryFullGraphContextType = {
  network?: Network;
  settings: SettingsType;
  visJsRef: React.RefObject<HTMLDivElement>;
  progressValue: number;
  setSettings: (settings: SettingsType) => void;
  onApply: () => void;
  datasets?: {
    nodes: DataSet<NodeDataType>;
    edges: DataSet<EdgeDataType>;
  };
};

const GraphContext = createContext<QueryFullGraphContextType>(
  {} as QueryFullGraphContextType
);

const edgeColors = [
  "rgba(166, 109, 161, 1)",
  "rgba(64, 74, 83, 1)",
  "rgba(90, 147, 189, 1)",
  "rgba(153,63,0,1)",
  "rgba(76,0,92,1)",
  "rgba(25,25,25,1)",
  "rgba(0,92,49,1)",
  "rgba(43,206,72,1)",
  "rgba(255,204,153,1)",
  "rgba(128,128,128,1)",
  "rgba(148,255,181,1)",
  "rgba(143,124,0,1)",
  "rgba(157,204,0,1)",
  "rgba(194,0,136,1)",
  "rgba(0,51,128,1)",
  "rgba(255,164,5,1)",
  "rgba(255,168,187,1)",
  "rgba(66,102,0,1)",
  "rgba(255,0,16,1)",
  "rgba(94,241,242,1)",
  "rgba(0,153,143,1)",
  "rgba(224,255,102,1)",
  "rgba(116,10,255,1)",
  "rgba(153,0,0,1)",
  "rgba(255,255,128,1)",
  "rgba(255,255,0,1)",
  "rgba(255,80,5,1)"
];
export const QueryFullGraphContextProvider = ({
  children,
  visJsRef,
  graphData
}: {
  children: ReactNode;
  visJsRef: React.RefObject<HTMLDivElement>;
  graphData?: { edges: EdgeDataType[]; nodes: NodeDataType[]; settings: any };
}) => {
  const [settings, setSettings] = React.useState({
    layout: "forceAtlas2",
    nodeColorByCollection: false,
    nodeColor: "#48BB78",
    edgeColor: "#1D2A12",
    edgeColorByCollection: false,
    edgeType: "solid"
  } as QueryFullGraphContextType["settings"]);
  const { progressValue, network, datasets } = useSetupQueryGraph({
    visJsRef,
    graphData,
    userSettings: settings
  });

  const onApply = React.useCallback(() => {
    if (settings.nodeColor && !settings.nodeColorByCollection) {
      const newNodes = datasets?.nodes.map(node => {
        node.color = settings.nodeColor;
        return node;
      });
      datasets?.nodes.update(newNodes || []);
    }
    if (settings.nodeColorByCollection) {
      // add group to nodes
      const newNodes = datasets?.nodes.map(node => {
        node.group = node.id.split("/")[0];
        node.color = undefined;
        return node;
      });
      datasets?.nodes.update(newNodes || []);
    }
    if (settings.edgeColor && !settings.edgeColorByCollection) {
      const newEdges = datasets?.edges.map(edge => {
        edge.color = settings.edgeColor;
        return edge;
      });
      datasets?.edges.update(newEdges || []);
    }
    if (settings.edgeColorByCollection) {
      let groupToColorMap: { [key: string]: string } = {};
      // add group to edges
      const newEdges = datasets?.edges.map(edge => {
        const group = edge.id.split("/")[0];
        // pick color from edgeColors based on group string
        if (!groupToColorMap[group]) {
          groupToColorMap[group] = edgeColors.pop() || "#000000";
        }
        edge.color = groupToColorMap[group];
        return edge;
      });
      datasets?.edges.update(newEdges || []);
    }
    if (settings.edgeType) {
      const dashes =
        settings.edgeType === "dashed"
          ? [5, 5]
          : settings.edgeType === "dotted"
          ? [1, 3]
          : false;
      const newEdges = datasets?.edges.map(edge => {
        edge.dashes = dashes;
        return edge;
      });
      datasets?.edges.update(newEdges || []);
    }
    const layoutOptions = getLayout(settings.layout);
    const newOptions = {
      ...layoutOptions
    };
    network?.setOptions(newOptions);
    network?.fit();
  }, [
    network,
    settings.layout,
    settings.nodeColor,
    settings.nodeColorByCollection,
    datasets?.nodes,
    datasets?.edges,
    settings.edgeColor,
    settings.edgeColorByCollection,
    settings.edgeType
  ]);
  return (
    <GraphContext.Provider
      value={{
        progressValue,
        network,
        settings,
        setSettings,
        onApply,
        visJsRef,
        datasets
      }}
    >
      {children}
    </GraphContext.Provider>
  );
};

export const useQueryFullGraph = () => {
  return useContext(GraphContext);
};
