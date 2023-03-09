import { CheckIcon, DeleteIcon } from "@chakra-ui/icons";
import { Box, Button, Stack, Text, useDisclosure } from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { CopyPropertiesDropdown } from "./CopyPropertiesDropdown";
import { useSearchAliasContext } from "./SearchAliasContext";

export const SearchAliasHeader = () => {
  const { view } = useSearchAliasContext();
  return (
    <Box padding="5" borderBottomWidth="2px" borderColor="gray.200">
      <Box display="grid" gridTemplateRows={"30px 1fr"}>
        <Text color="gray.700" fontWeight="600" fontSize="lg">
          {view.name}
        </Text>
        <Box display="grid" gridTemplateColumns="1fr 0.5fr">
          <CopyPropertiesDropdown />
          <ActionButtons />
        </Box>
      </Box>
    </Box>
  );
};

const ActionButtons = () => {
  const { onSave, errors, changed } = useSearchAliasContext();
  return (
    <Box display={"flex"} justifyContent="end" alignItems={"center"} gap="4">
      <Button
        size="xs"
        colorScheme="blue"
        leftIcon={<CheckIcon />}
        onClick={onSave}
        isDisabled={errors.length > 0 || !changed}
      >
        Save view
      </Button>
      <DeleteViewButton />
    </Box>
  );
};

const DeleteViewButton = () => {
  const { onDelete, view } = useSearchAliasContext();
  const { onOpen, onClose, isOpen } = useDisclosure();
  return (
    <>
      <Button
        size="xs"
        colorScheme="red"
        leftIcon={<DeleteIcon />}
        onClick={onOpen}
      >
        Delete
      </Button>
      <Modal isOpen={isOpen} onClose={onClose}>
        <ModalHeader>Delete view: {view.name}?</ModalHeader>
        <ModalBody>
          <p>
            Are you sure? Clicking on the <b>Delete</b> button will permanently
            delete this view.
          </p>
        </ModalBody>
        <ModalFooter>
          <Stack direction="row">
            <Button colorScheme={"gray"} onClick={onClose}>
              Close
            </Button>
            <Button colorScheme="red" onClick={onDelete}>
              Delete
            </Button>
          </Stack>
        </ModalFooter>
      </Modal>
    </>
  );
};
