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
import { notifyError, notifySuccess } from "../../../utils/notifications";
import { DatabaseDescription } from "../Database.types";

export const DeleteDatabaseModal = ({
  database,
  onClose,
  onSuccess
}: {
  database: DatabaseDescription;
  onClose: () => void;
  onSuccess: () => void;
}) => {
  const [isLoading, setIsLoading] = React.useState(false);
  const onDelete = async () => {
    const currentDB = getCurrentDB();
    setIsLoading(true);
    try {
      await currentDB.dropDatabase(database.name);
      notifySuccess(
        `The database: "${database.name}" was successfully deleted`
      );
      mutate("/databases");
      setIsLoading(false);
      onSuccess();
    } catch (e: any) {
      notifyError(
        `Could not delete database: ${e.response.parsedBody.errorMessage}`
      );
      setIsLoading(false);
    }
  };
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete Database</ModalHeader>
      <ModalBody>
        Are you sure you want to delete the database: {database.name}?
      </ModalBody>
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
