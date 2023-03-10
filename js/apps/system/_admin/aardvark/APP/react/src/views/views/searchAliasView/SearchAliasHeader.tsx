import { CheckIcon, DeleteIcon, EditIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  IconButton,
  Input,
  Stack,
  Text,
  useDisclosure
} from "@chakra-ui/react";
import React, { useState } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { CopyPropertiesDropdown } from "./CopyPropertiesDropdown";
import { useSearchAliasContext } from "./SearchAliasContext";
import { putRenameView } from "./useUpdateAliasViewProperties";

export const SearchAliasHeader = () => {
  return (
    <Box padding="4" borderBottomWidth="2px" borderColor="gray.200">
      <Box display="grid" gap="4" gridTemplateRows={"30px 1fr"}>
        <EditableNameField />
        <Box display="grid" gridTemplateColumns="1fr 0.5fr">
          <CopyPropertiesDropdown />
          <ActionButtons />
        </Box>
      </Box>
    </Box>
  );
};

const ActionButtons = () => {
  const { onSave, errors, changed, isAdminUser } = useSearchAliasContext();
  return (
    <Box display={"flex"} justifyContent="end" alignItems={"center"} gap="4">
      <Button
        size="xs"
        colorScheme="green"
        leftIcon={<CheckIcon />}
        onClick={onSave}
        isDisabled={errors.length > 0 || !changed || !isAdminUser}
      >
        Save view
      </Button>
      <DeleteViewButton />
    </Box>
  );
};

const DeleteViewButton = () => {
  const { onDelete, view, isAdminUser } = useSearchAliasContext();
  const { onOpen, onClose, isOpen } = useDisclosure();
  return (
    <>
      <Button
        size="xs"
        colorScheme="red"
        leftIcon={<DeleteIcon />}
        onClick={onOpen}
        isDisabled={!isAdminUser}
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

const EditableNameField = () => {
  const { initialView, isAdminUser, isCluster } = useSearchAliasContext();
  const { onOpen, onClose, isOpen } = useDisclosure();
  const [newName, setNewName] = useState(initialView.name);
  const [loading, setLoading] = useState(false);
  if (isOpen) {
    return (
      <Stack
        as="form"
        margin="0"
        direction="row"
        alignItems="center"
        onSubmit={async event => {
          event.preventDefault();
          setLoading(true);
          try {
            const isError = await putRenameView({
              initialName: initialView.name,
              name: newName
            });
            setLoading(false);
            if (!isError) {
              onClose();
              let newRoute = `#view/${newName}`;
              window.App.navigate(newRoute, {
                trigger: true,
                replace: true
              });
            }
          } catch (e) {
            setLoading(false);
            window.arangoHelper.arangoError(
              "Failure",
              `Got unexpected server response: ${e.message}`
            );
          }
        }}
      >
        <Input
          autoFocus
          value={newName}
          backgroundColor={"white"}
          isDisabled={loading}
          onChange={e => setNewName(e.target.value)}
          maxWidth="300px"
          placeholder="Enter name"
        />
        <IconButton
          type="submit"
          isLoading={loading}
          aria-label="Edit name"
          icon={<CheckIcon />}
        />
      </Stack>
    );
  }
  return (
    <Stack direction="row" alignItems="center">
      <Text color="gray.700" fontWeight="600" fontSize="lg">
        {initialView.name} {!isAdminUser ? "(read only)" : null}
      </Text>
      {!isCluster && isAdminUser ? <IconButton
        aria-label="Open edit name input"
        icon={<EditIcon />}
        onClick={onOpen}
      /> : null}
    </Stack>
  );
};
