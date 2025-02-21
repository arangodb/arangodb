import { Button, HStack } from "@chakra-ui/react";
import React from "react";
import { Modal, ModalFooter, ModalHeader } from "../../../../components/modal";
import { getCurrentDB } from "../../../../utils/arangoClient";
import { SelectedActionType } from "../GraphAction.types";
import { useGraph } from "../GraphContext";
import { useEdgeData } from "./useEdgeData";

const useDeleteEdgeAction = (selectedAction?: SelectedActionType) => {
  const { edgeId } = selectedAction?.entity || {};
  const { edgeData } = useEdgeData({ edgeId });
  const { datasets, onClearAction } = useGraph();

  const deleteEdge = (edgeId: string) => {
    const slashPos = edgeId.indexOf("/");
    getCurrentDB()
      .collection(edgeId.substring(0, slashPos))
      .remove(edgeId.substring(slashPos + 1))
      .then(() => {
        window.arangoHelper.arangoNotification(
          `The edge ${edgeId} was successfully deleted`
        );
        datasets?.edges.remove(edgeId);
        onClearAction();
      })
      .catch(() => {
        console.log("ERROR: Could not delete edge");
      });
  };
  return { edgeId, edgeData, deleteEdge };
};
export const DeleteEdgeModal = () => {
  const { selectedAction, onClearAction } = useGraph();
  const { edgeId, deleteEdge } = useDeleteEdgeAction(selectedAction);

  if (!edgeId) {
    return null;
  }

  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Delete Edge: {edgeId}?</ModalHeader>
      <ModalFooter>
        <HStack>
          <Button onClick={onClearAction}>Cancel</Button>
          <Button
            colorScheme="red"
            onClick={() => {
              deleteEdge(edgeId);
            }}
          >
            Delete
          </Button>
        </HStack>
      </ModalFooter>
    </Modal>
  );
};
