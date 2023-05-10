import { ViewIcon } from "@chakra-ui/icons";
import { Box, IconButton, useDisclosure } from "@chakra-ui/react";
import React from "react";
import { Modal, ModalBody, ModalHeader } from "../../../components/modal";
import { InvertedIndexView } from "./addIndex/invertedIndex/InvertedIndexView";
import { IndexRowType } from "./useFetchIndices";

export const ViewIndexButton = ({ indexRow }: { indexRow: IndexRowType }) => {
  const { onOpen, onClose, isOpen } = useDisclosure();
  return (
    <Box display="flex" justifyContent="center">
      <IconButton
        colorScheme="gray"
        variant="ghost"
        size="sm"
        aria-label="View Index"
        icon={<ViewIcon />}
        onClick={onOpen}
      />
      <ViewModal indexRow={indexRow} onClose={onClose} isOpen={isOpen} />
    </Box>
  );
};
const ViewModal = ({
  onClose,
  isOpen,
  indexRow
}: {
  onClose: () => void;
  isOpen: boolean;
  indexRow: IndexRowType;
}) => {
  return (
    <Modal size="max" onClose={onClose} isOpen={isOpen}>
      <ModalHeader>
        Index: "{indexRow.name}" (ID: {indexRow.id})
      </ModalHeader>
      <ModalBody>
        <InvertedIndexView onClose={onClose} indexRow={indexRow} />
      </ModalBody>
    </Modal>
  );
};
