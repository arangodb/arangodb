import { createContext, Dispatch, SetStateAction, useContext } from "react";

export const DEFAULT_URL_PARAMETERS = {
  depth: 2,
  limit: 250,
  fruchtermann: "fruchtermann",
  nodeColorByCollection: true,
  edgeColorByCollection: true,
  nodeColor: "48BB78",
  nodeColorAttribute: "",
  edgeColor: "1D2A12",
  edgeColorAttribute: "",
  nodeLabel: "",
  edgeLabel: "",
  edgeDirection: true,
  edgeType: "line",
  nodeSize: "",
  nodeSizeByEdges: false,
  edgeEditable: true,
  nodeLabelByCollection: true,
  edgeLabelByCollection: true,
  nodeStart: "",
  barnesHutOptimize: false,
  query: "",
  layout: "forceAtlas2" as LayoutType
};

export type LayoutType = "forceAtlas2" | "hierarchical";

export type UrlParametersType = {
  depth: number;
  limit: number;
  fruchtermann: string;
  nodeColorByCollection: boolean;
  edgeColorByCollection: boolean;
  nodeColor: string;
  nodeColorAttribute: string;
  edgeColor: string;
  edgeColorAttribute: string;
  nodeLabel: string;
  edgeLabel: string;
  edgeDirection: boolean;
  edgeType: string;
  nodeSize: string;
  nodeSizeByEdges: boolean;
  edgeEditable: boolean;
  nodeLabelByCollection: boolean;
  edgeLabelByCollection: boolean;
  nodeStart?: string;
  barnesHutOptimize: boolean;
  query: string;
  layout: LayoutType;
};

type UrlParametersContextType = {
  urlParams: UrlParametersType;
  setUrlParams: Dispatch<SetStateAction<UrlParametersType>>;
};

export const UrlParametersContext = createContext<UrlParametersContextType>({
  urlParams: DEFAULT_URL_PARAMETERS
} as UrlParametersContextType);

export const useUrlParameterContext = () => useContext(UrlParametersContext);
