import { Button, Stack } from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useQueryContext } from "../QueryContextProvider";
import { QueryType } from "./useFetchUserSavedQueries";

export const DeleteQueryModal = ({
  query,
  isOpen,
  onClose
}: {
  query: QueryType | null;
  isOpen: boolean;
  onClose: () => void;
}) => {
  const { onDelete } = useQueryContext();
  if (!query) return null;
  return (
    <Modal isOpen={isOpen} onClose={onClose}>
      <ModalHeader>Delete Query</ModalHeader>
      <ModalBody>
        Are you sure you want to delete the query: {query.name}?
      </ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button colorScheme="gray" onClick={() => onClose()}>
            Cancel
          </Button>
          <Button
            colorScheme="red"
            onClick={async () => {
              await onDelete(query.name);
              onClose();
            }}
          >
            Delete
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
