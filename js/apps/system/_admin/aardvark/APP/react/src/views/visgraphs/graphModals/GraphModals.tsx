import React from "react";
import { useGraph } from "../GraphContext";
import { DeleteEdgeModal } from "./DeleteEdgeModal";
import { DeleteNodeModal } from "./DeleteNodeModal";
import { EditNodeModal } from "./EditNodeModal";

export const GraphModals = () => {
  const { selectedAction } = useGraph();
  if (selectedAction?.entity.edgeId) {
    if (selectedAction?.action === "delete") {
      return <DeleteEdgeModal />;
    }
    if (selectedAction?.action === "edit") {
      return <DeleteEdgeModal />;
    }
  }

  if (selectedAction?.entity.nodeId) {
    if (selectedAction?.action === "delete") {
      return <DeleteNodeModal />;
    }
    if (selectedAction?.action === "edit") {
      return <EditNodeModal />;
    }
  }

  return null;
};
