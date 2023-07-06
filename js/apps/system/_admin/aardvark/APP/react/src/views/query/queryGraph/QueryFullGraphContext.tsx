import React, { createContext, ReactNode, useContext } from "react";
import { Network } from "vis-network";
import { EdgeDataType, NodeDataType } from "../../graphV2/GraphData.types";
import { LayoutType } from "../../graphV2/UrlParametersContext";
import { useSetupQueryGraph } from "../queryResults/useSetupQueryGraph";
type SettingsType = {
  layout: LayoutType;
};
type QueryFullGraphContextType = {
  network?: Network;
  settings: SettingsType;
  visJsRef: React.RefObject<HTMLDivElement>;
  progressValue: number;
  setSettings: (settings: SettingsType) => void;
  onApply: () => void;
};

const GraphContext = createContext<QueryFullGraphContextType>(
  {} as QueryFullGraphContextType
);
const getLayout = (layout: LayoutType) => {
  const hierarchicalOptions = {
    layout: {
      randomSeed: 0,
      hierarchical: {
        levelSeparation: 150,
        nodeSpacing: 300,
        direction: "UD"
      }
    },
    physics: {
      barnesHut: {
        gravitationalConstant: -2250,
        centralGravity: 0.4,
        damping: 0.095
      },
      solver: "barnesHut"
    }
  };

  const forceAtlas2BasedOptions = {
    layout: {
      randomSeed: 0,
      hierarchical: false
    },
    physics: {
      forceAtlas2Based: {
        springLength: 10,
        springConstant: 1.5,
        gravitationalConstant: -500
      },
      minVelocity: 0.75,
      solver: "forceAtlas2Based"
    }
  };
  switch (layout) {
    case "hierarchical":
      return hierarchicalOptions;
    case "forceAtlas2":
    default:
      return forceAtlas2BasedOptions;
  }
};
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
    layout: "forceAtlas2"
  } as QueryFullGraphContextType["settings"]);
  const { progressValue, network } = useSetupQueryGraph({
    visJsRef,
    graphData,
    userSettings: settings
  });

  const onApply = React.useCallback(() => {
    let layoutOptions = getLayout(settings.layout);
    console.log({ layoutOptions });
    network?.setOptions({
      ...layoutOptions
    });
    network?.fit();
  }, [network, settings.layout]);
  return (
    <GraphContext.Provider
      value={{
        progressValue,
        network,
        settings,
        setSettings,
        onApply,
        visJsRef
      }}
    >
      {children}
    </GraphContext.Provider>
  );
};

export const useQueryFullGraph = () => {
  return useContext(GraphContext);
};
