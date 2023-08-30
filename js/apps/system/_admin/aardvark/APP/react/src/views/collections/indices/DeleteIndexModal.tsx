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
  index,
  onClose,
  onSuccess
}: {
  index: Index;
  onClose: () => void;
  onSuccess: () => void;
}) => {
  const { collectionId } = useCollectionIndicesContext();
  const { onDeleteIndex } = useDeleteIndex({ collectionId });
  const onDelete = async () => {
    onDeleteIndex({ id: index.id, onSuccess });
  };
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete Index</ModalHeader>
      <ModalBody>Are you sure you want to delete {index.name}?</ModalBody>
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
