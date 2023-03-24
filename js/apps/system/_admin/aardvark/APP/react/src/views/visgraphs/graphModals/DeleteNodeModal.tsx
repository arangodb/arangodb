import { Button, Checkbox, HStack, Stack } from "@chakra-ui/react";
import React, { useState } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import {
  getCurrentDB
} from "../../../utils/arangoClient";
import { SelectedActionType, useGraph } from "../GraphContext";
import { AttributesInfo } from "./AttributesInfo";
import { useNodeData } from "./useNodeData";

const useDeleteNodeAction = ({
  selectedAction,
  deleteEdges,
  graphName
}: {
  selectedAction?: SelectedActionType;
  deleteEdges: boolean;
  graphName: string;
}) => {
  const { nodeId } = selectedAction?.entity || {};
  const { nodeData } = useNodeData({ nodeId });
  const deleteNode = async (nodeId: string) => {
    const slashPos = nodeId.indexOf("/");
    const collection = nodeId.substring(0, slashPos);
    const vertex = nodeId.substring(slashPos + 1);
    const data = {
      keys: [vertex],
      collection: collection
    };
    const db = getCurrentDB();
    if (deleteEdges) {
      const graphCollection = db.graph(graphName).vertexCollection(collection);
      try {
        await graphCollection.remove(vertex);
        window.arangoHelper.arangoNotification(
          `The node ${nodeId} and connected edges were successfully deleted`
        );
      } catch (e) {
        console.log("Error: ", e);
        window.arangoHelper.arangoError("Graph", "Could not delete node.");
        console.log("ERROR: Could not delete edge");
      }
    } else {
      const keys = data.keys;
      const dbCollection = db.collection(collection);
      try {
        await dbCollection.removeByKeys(keys);
        window.arangoHelper.arangoNotification(
          `The node ${nodeId} was successfully deleted`
        );
      } catch (e) {
        console.log("Error: ", e);
        window.arangoHelper.arangoError("Graph", "Could not delete node.");
        console.log("ERROR: Could not delete edge");
      }
    }
  };
  return { nodeId, nodeData, deleteNode };
};

export const DeleteNodeModal = () => {
  const { graphName, selectedAction, onClearAction } = useGraph();
  const [deleteEdges, setDeleteEdges] = useState(true);
  const { nodeId, nodeData, deleteNode } = useDeleteNodeAction({
    selectedAction,
    deleteEdges,
    graphName
  });

  if (!nodeId) {
    return null;
  }

  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Delete Node: {nodeId}?</ModalHeader>
      <ModalBody>
        <Stack spacing="4">
          <AttributesInfo attributes={nodeData} />
          <Checkbox
            isChecked={deleteEdges}
            onChange={() => {
              setDeleteEdges(deleteEdges => !deleteEdges);
            }}
          >
            Delete connected edges too
          </Checkbox>
        </Stack>
      </ModalBody>
      <ModalFooter>
        <HStack>
          <Button onClick={onClearAction}>Cancel</Button>
          <Button
            colorScheme="red"
            onClick={() => {
              deleteNode(nodeId);
            }}
          >
            Delete
          </Button>
        </HStack>
      </ModalFooter>
    </Modal>
  );
};
