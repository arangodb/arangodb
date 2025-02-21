import {
  Button,
  ModalBody,
  ModalFooter,
  ModalHeader,
  Stack
} from "@chakra-ui/react";
import React from "react";
import { mutate } from "swr";
import { Modal } from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { ViewDescription } from "../View.types";

export const DeleteViewModal = ({
  view,
  onClose,
  onSuccess
}: {
  view: ViewDescription;
  onClose: () => void;
  onSuccess: () => void;
}) => {
  const [isLoading, setIsLoading] = React.useState(false);
  const onDelete = async () => {
    const currentDB = getCurrentDB();
    setIsLoading(true);
    try {
      await currentDB.view(view.name).drop();
      await mutate("/views");
      setIsLoading(false);
      onSuccess();
    } catch (e: any) {
      window.arangoHelper.arangoError(
        "View",
        `Could not delete view: ${e.response.parsedBody.errorMessage}`
      );
      setIsLoading(false);
    }
  };
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete View</ModalHeader>
      <ModalBody>Are you sure you want to delete {view.name}?</ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button isDisabled={isLoading} colorScheme="gray" onClick={onClose}>
            Cancel
          </Button>
          <Button isLoading={isLoading} colorScheme="red" onClick={onDelete}>
            Delete
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
