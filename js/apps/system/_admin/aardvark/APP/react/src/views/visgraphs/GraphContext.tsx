import { ArangojsResponse } from "arangojs/lib/request.node";
import React, {
  createContext,
  ReactNode,
  useCallback,
  useContext,
  useEffect,
  useState
} from "react";
import { getRouteForDB } from "../../utils/arangoClient";
import { loadingNode } from "./data";

const GraphContext = createContext<{
  graphData: VisGraphData;
  graphName: string;
}>({
  graphData: {} as VisGraphData,
  graphName: ""
});

type NodeType = {
  id: string;
  label: string;
  size: number;
  value: number;
  sizeCategory: string;
  shape: string;
  color?: string;
  font: {
    strokeWidth: number;
    strokeColor: string;
    vadjust: number;
  };
  sortColor: string;
  borderWidth: number;
  shadow?: {
    enabled: true;
    color: string;
    size: number;
    x: number;
    y: number;
  };
};

type EdgeType = {
  id: string;
  source: string;
  from: string;
  label: string;
  target: string;
  to: string;
  color: string;
  font: {
    strokeWidth: number;
    strokeColor: string;
    align: string;
  };
  length: number;
  size: number;
  sortColor: string;
};

type SettingsType = {
  layout: {
    interaction: {
      dragNodes: boolean;
      dragView: boolean;
      hideEdgesOnDrag: boolean;
      hideNodesOnDrag: boolean;
      hover: boolean;
      hoverConnectedEdges: boolean;
      keyboard: {
        enabled: boolean;
        speed: {
          x: number;
          y: number;
          zoom: number;
        };
        bindToWindow: boolean;
      };
      multiselect: boolean;
      navigationButtons: boolean;
      selectable: boolean;
      selectConnectedEdges: boolean;
      tooltipDelay: number;
      zoomSpeed: number;
      zoomView: boolean;
    };
    layout: {
      randomSeed: number;
      hierarchical: boolean;
    };
    edges: {
      smooth: boolean;
      arrows: {
        to: {
          enabled: boolean;
          type: string;
        };
      };
    };
    physics: {
      barnesHut: {
        gravitationalConstant: number;
        centralGravity: number;
        springLength: number;
        damping: number;
      };
      solver: string;
    };
  };
  vertexCollections: {
    name: string;
    id: string;
  }[];
  edgesCollections: {
    name: string;
    id: string;
  }[];
  startVertex: {
    _key: string;
    _id: string;
    _rev: string;
    name: string;
  };
  // todo, check these
  nodesColorAttributes: string[];
  edgesColorAttributes: string[];
  nodesSizeValues: number[];
  nodesSizeMinMax: (number | null)[];
  connectionsCounts: number[];
  connectionsMinMax: (number | null)[];
};
type VisGraphData = {
  nodes: NodeType[];
  edges: EdgeType[];
  settings: SettingsType;
};
interface VisDataResponse extends ArangojsResponse {
  body?: VisGraphData;
}
export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);

  let [visGraphData, setVisGraphData] = useState(loadingNode);

  const fetchVisData = useCallback(() => {
    getRouteForDB(window.frontendConfig.db, "_admin")
      .get(`/aardvark/visgraph/${graphName}`)
      .then((data: VisDataResponse) => {
        if (data.body) {
          setVisGraphData(data.body);
        }
      })
      .catch(error => {
        window.arangoHelper.arangoError(
          "Graph",
          error.responseJSON.errorMessage
        );
        console.log(error);
      });
  }, [graphName]);

  useEffect(() => {
    fetchVisData();
  }, [fetchVisData]);

  return (
    <GraphContext.Provider value={{ graphData: visGraphData, graphName }}>
      {children}
    </GraphContext.Provider>
  );
};

export const useGraph = () => {
  return useContext(GraphContext);
};
