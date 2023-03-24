import { Button, Flex, Spacer } from "@chakra-ui/react";
import React from "react";
import { Modal, ModalBody, ModalHeader } from "../../components/modal";
import { useGraph } from "./GraphContext";

export const DeleteEdgeModal = () => {
  const { deleteEdgeModalData, onCancelDelete } = useGraph();
  const { edgeId } = deleteEdgeModalData || {};
  console.log({edgeId}, 'modal!');
  const openNotificationWithIcon = () => {
    window.arangoHelper.arangoNotification(
      `The edge ${edgeId} was successfully deleted`
    );
  };

  const deleteEdge = (edgeId: string) => {
    const slashPos = edgeId.indexOf("/");
    const data = {
      keys: [edgeId.substring(slashPos + 1)],
      collection: edgeId.substring(0, slashPos)
    };

    $.ajax({
      cache: false,
      type: "PUT",
      contentType: "application/json",
      data: JSON.stringify(data),
      url: window.arangoHelper.databaseUrl("/_api/simple/remove-by-keys"),
      success: function() {
        openNotificationWithIcon();
      },
      error: function() {
        console.log("ERROR: Could not delete edge");
      }
    });
  };

  if (!edgeId) {
    return null;
  }

  return (
    <Modal size="6xl" isOpen onClose={onCancelDelete}>
      <ModalHeader>Delete Edge: {edgeId}?</ModalHeader>
      <ModalBody>
        <Flex direction="row" mt="38">
          <Spacer />
          <Button onClick={onCancelDelete}>Cancel</Button>
          <Button
            colorScheme="red"
            onClick={() => {
              deleteEdge(edgeId);
            }}
          >
            Delete
          </Button>
        </Flex>
      </ModalBody>
    </Modal>
  );
};
