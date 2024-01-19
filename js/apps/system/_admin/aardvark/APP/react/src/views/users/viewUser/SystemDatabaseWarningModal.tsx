import { Button, Stack } from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useUserPermissionsContext } from "./UserPermissionsContext";

export const SystemDatabaseWarningModal = () => {
  const {
    systemDatabaseActionState,
    onConfirmSystemDatabasePermissionChange,
    onCancelSystemDatabasePermissionChange
  } = useUserPermissionsContext();
  const { status } = systemDatabaseActionState || {};
  if (status !== "open") {
    return null;
  }
  return (
    <Modal isOpen onClose={onCancelSystemDatabasePermissionChange}>
      <ModalHeader>Warning</ModalHeader>
      <ModalBody>
        You are changing the "_system" database permission. Really continue?
      </ModalBody>
      <ModalFooter>
        <Stack direction="row" spacing={4} align="center">
          <Button
            colorScheme="gray"
            size="sm"
            onClick={onCancelSystemDatabasePermissionChange}
          >
            Cancel
          </Button>
          <Button
            colorScheme="orange"
            size="sm"
            onClick={onConfirmSystemDatabasePermissionChange}
          >
            Yes, change permission
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
