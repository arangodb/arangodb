import { Edge, Node, Options } from "vis-network";

export interface NodeDataType extends Node {
  id: string;
  sizeCategory?: string;
  sortColor?: string;
}

export interface EdgeDataType extends Edge {
  id: string;
  from: string;
  to: string;
  source?: string;
  target?: string;
  sortColor?: string;
}

// options need to be typed manually as
// they are marked as `any` in vis-network exports
interface GraphOptions extends Options {
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
}

interface SettingsType {
  layout: GraphOptions;
  nodeColorAttributeMessage?: string;
  edgeColorAttributeMessage?: string;
  nodeSizeAttributeMessage?: string;
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
  isSmart?: boolean;
  smartGraphAttribute?: string;
}

export type GraphDataType = {
  nodes: NodeDataType[];
  edges: EdgeDataType[];
  settings: SettingsType;
};
