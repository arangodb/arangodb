import { Box, Button, HStack, Tag } from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { SelectedActionType } from "../GraphAction.types";
import { useGraph } from "../GraphContext";
import { useEdgeData } from "./useEdgeData";

const useDeleteEdgeAction = (selectedAction?: SelectedActionType) => {
  const { edgeId } = selectedAction?.entity || {};
  const { edgeData } = useEdgeData({ edgeId });
  const { datasets, onClearAction } = useGraph();

  const deleteEdge = (edgeId: string) => {
    const slashPos = edgeId.indexOf("/");
    const data = {
      keys: [edgeId.substring(slashPos + 1)],
      collection: edgeId.substring(0, slashPos)
    };

    getApiRouteForCurrentDB()
      .put("/simple/remove-by-keys", data)
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
  const { edgeId, edgeData, deleteEdge } = useDeleteEdgeAction(selectedAction);

  if (!edgeId) {
    return null;
  }

  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Delete Edge: {edgeId}?</ModalHeader>
      <ModalBody>
        <AttributesInfo attributes={edgeData} />
      </ModalBody>
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

const AttributesInfo = ({
  attributes
}: {
  attributes?: { [key: string]: string };
}) => {
  if (!attributes) {
    return null;
  }
  return (
    <Box>
      {Object.keys(attributes).map(key => (
        <Tag key={key}>{`${key}: ${JSON.stringify(attributes[key])}`}</Tag>
      ))}
    </Box>
  );
};
