import { CheckIcon, DeleteIcon } from "@chakra-ui/icons";
import { Box, Button, Stack, useDisclosure } from "@chakra-ui/react";
import { useField, useFormikContext } from "formik";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useEditViewContext } from "../editView/EditViewContext";
import { ViewPropertiesType } from "../searchView.types";
import { CopyPropertiesDropdown } from "./CopyPropertiesDropdown";
import { EditableViewNameField } from "./EditableViewNameField";

export const EditViewHeader = () => {
  const { isAdminUser } = useEditViewContext();
  return (
    <Box padding="4" borderBottomWidth="2px" borderColor="gray.200">
      <Box display="grid" gap="4" gridTemplateRows={"30px 1fr"}>
        <EditableNameFieldWrap />
        {isAdminUser && (
          <Box display="grid" gridTemplateColumns="1fr 0.5fr">
            <CopyPropertiesDropdown />
            <ActionButtons />
          </Box>
        )}
      </Box>
    </Box>
  );
};

const ActionButtons = () => {
  const { errors, changed, isAdminUser } = useEditViewContext();
  const { submitForm } = useFormikContext<ViewPropertiesType>();
  return (
    <Box display={"flex"} justifyContent="end" alignItems={"center"} gap="4">
      <Button
        size="xs"
        colorScheme="green"
        leftIcon={<CheckIcon />}
        onClick={submitForm}
        isDisabled={errors.length > 0 || !changed || !isAdminUser}
      >
        Save view
      </Button>
      <DeleteViewButton />
    </Box>
  );
};

const DeleteViewButton = () => {
  const { onDelete, isAdminUser } = useEditViewContext();
  const { values: view } = useFormikContext<ViewPropertiesType>();
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

const EditableNameFieldWrap = () => {
  const { isAdminUser, isCluster } = useEditViewContext();
  const { values } = useFormikContext<ViewPropertiesType>();
  const [, , helpers] = useField("name");
  return (
    <EditableViewNameField
      view={values}
      isAdminUser={isAdminUser}
      isCluster={isCluster}
      setCurrentName={(name: string) => {
        helpers.setValue(name);
      }}
    />
  );
};
