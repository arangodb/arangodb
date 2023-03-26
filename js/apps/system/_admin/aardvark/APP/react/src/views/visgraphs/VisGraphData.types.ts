import { ArangojsResponse } from "arangojs/lib/request.node";

export type NodeDataType = {
  id: string;
  label: string;
  size: number;
  value: number;
  sizeCategory?: string;
  shape: string;
  color?: string;
  fixed?: boolean;
  font: {
    strokeWidth: number;
    strokeColor: string;
    vadjust: number;
  };
  sortColor?: string;
  borderWidth?: number;
  shadow?: {
    enabled: true;
    color: string;
    size: number;
    x: number;
    y: number;
  };
};
export type EdgeDataType = {
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
export type VisGraphData = {
  nodes: NodeDataType[];
  edges: EdgeDataType[];
  settings: SettingsType;
};
export interface VisDataResponse extends ArangojsResponse {
  body?: VisGraphData;
}
