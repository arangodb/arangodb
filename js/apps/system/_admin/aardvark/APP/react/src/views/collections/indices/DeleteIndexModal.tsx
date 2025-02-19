import { Button, Stack } from "@chakra-ui/react";
import { HiddenIndex, Index } from "arangojs/indexes";
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
  foundCollectionIndex: HiddenIndex;
  onClose: () => void;
  onSuccess: () => void;
}) => {
  const { collectionId, collectionName } = useCollectionIndicesContext();
  const { onDeleteIndex } = useDeleteIndex({ collectionId, collectionName });
  const onDelete = async () => {
    onDeleteIndex({ id: foundCollectionIndex.id, onSuccess });
  };
  const name = (foundCollectionIndex as Index).name || foundCollectionIndex.id;
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete Index</ModalHeader>
      <ModalBody>Are you sure you want to delete {name}?</ModalBody>
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
