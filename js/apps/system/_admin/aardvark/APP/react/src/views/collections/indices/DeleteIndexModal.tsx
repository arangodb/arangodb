import { Button, Stack } from "@chakra-ui/react";
import { Index } from "arangojs/indexes";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";
import { useDeleteIndex } from "./useDeleteIndex";

export const DeleteIndexModal = ({
  foundCollectionIndex,
  onClose,
  onSuccess
}: {
  foundCollectionIndex: Index;
  onClose: () => void;
  onSuccess: () => void;
}) => {
  const { collectionId, collectionName } = useCollectionIndicesContext();
  const { onDeleteIndex } = useDeleteIndex({ collectionId, collectionName });
  const onDelete = async () => {
    onDeleteIndex({ id: foundCollectionIndex.id, onSuccess });
  };
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete Index</ModalHeader>
      <ModalBody>
        Are you sure you want to delete {foundCollectionIndex.name}?
      </ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button colorScheme="gray" onClick={() => onClose()}>
            Cancel
          </Button>
          <Button colorScheme="red" onClick={onDelete}>
            Delete
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
