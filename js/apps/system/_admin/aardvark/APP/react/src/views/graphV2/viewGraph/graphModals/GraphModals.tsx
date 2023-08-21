import React from "react";
import { useGraph } from "../GraphContext";
import { AddNodeModal } from "./AddNodeModal";
import { DeleteEdgeModal } from "./DeleteEdgeModal";
import { DeleteNodeModal } from "./DeleteNodeModal";
import { EditEdgeModal } from "./EditEdgeModal";
import { EditNodeModal } from "./EditNodeModal";
import { AddEdgeHandler } from "./AddEdgeHandler";
import { LoadFullGraphModal } from "./LoadFullGraphModal";

export const GraphModals = () => {
  const { selectedAction } = useGraph();
  if (selectedAction?.entity?.edgeId) {
    if (selectedAction?.action === "delete") {
      return <DeleteEdgeModal />;
    }
    if (selectedAction?.action === "edit") {
      return <EditEdgeModal />;
    }
  }

  if (selectedAction?.entity?.nodeId) {
    if (selectedAction?.action === "delete") {
      return <DeleteNodeModal />;
    }
    if (selectedAction?.action === "edit") {
      return <EditNodeModal />;
    }
  }

  if (selectedAction?.action === "add") {
    if (selectedAction.entityType === "node") {
      return <AddNodeModal />;
    }
    if (selectedAction.entityType === "edge") {
      return <AddEdgeHandler />;
    }
  }

  if (selectedAction?.action === "loadFullGraph") {
    return <LoadFullGraphModal />;
  }

  return null;
};
