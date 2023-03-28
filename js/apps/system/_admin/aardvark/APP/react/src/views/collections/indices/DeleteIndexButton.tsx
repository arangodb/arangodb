import { DeleteIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  IconButton,
  Stack,
  useDisclosure
} from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useCollectionIndicesContext } from "./CollectionIndicesContext";
import { IndexRowType } from "./useFetchIndices";

export const DeleteIndexButton = ({ indexRow }: { indexRow: IndexRowType }) => {
  const { onOpen, onClose, isOpen } = useDisclosure();
  const { readOnly } = useCollectionIndicesContext();
  return (
    <Box display="flex" justifyContent="center">
      <IconButton
        isDisabled={readOnly}
        colorScheme="red"
        variant="ghost"
        size="sm"
        aria-label="Delete Index"
        icon={<DeleteIcon />}
        onClick={onOpen}
      />
      <DeleteModal indexRow={indexRow} onClose={onClose} isOpen={isOpen} />
    </Box>
  );
};
const DeleteModal = ({
  onClose,
  isOpen,
  indexRow
}: {
  onClose: () => void;
  isOpen: boolean;
  indexRow: IndexRowType;
}) => {
  const { onDeleteIndex } = useCollectionIndicesContext();
  return (
    <Modal onClose={onClose} isOpen={isOpen}>
      <ModalHeader>
        Delete Index: "{indexRow.name}" (ID: {indexRow.id})?
      </ModalHeader>
      <ModalBody></ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button onClick={onClose}>Cancel</Button>
          <Button
            colorScheme="red"
            onClick={() =>
              onDeleteIndex({ id: indexRow.id, onSuccess: onClose })
            }
          >
            Delete
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
