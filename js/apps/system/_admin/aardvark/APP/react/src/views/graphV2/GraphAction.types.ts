import { EdgeDataType } from "./GraphData.types";

export type GraphPointer = {
  DOM: { x: number; y: number };
  canvas: { x: number; y: number };
};

export type RightClickedEntityType = {
  type: string;
  nodeId?: string;
  edgeId?: string;
  pointer: GraphPointer;
};
export type SelectedActionType = {
  action: "delete" | "edit" | "add" | "loadFullGraph";
  entityType?: "node" | "edge";
  from?: string;
  to?: string;
  callback?: (edge: EdgeDataType) => void;
  entity?: RightClickedEntityType;
};

export type SelectedEntityType = { entityId: string; type: "node" | "edge" };
