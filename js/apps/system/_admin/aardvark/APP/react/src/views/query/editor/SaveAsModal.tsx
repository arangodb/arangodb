import { Button, FormLabel, Input, Stack } from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useQueryContext } from "../QueryContextProvider";

export const SaveAsModal = () => {
  const [newQueryName, setNewQueryName] = React.useState<string>("");
  const {
    onSaveAs,
    onSave,
    setQueryName,
    savedQueries,
    isSaveAsModalOpen,
    onCloseSaveAsModal
  } = useQueryContext();
  const queryExists = !!savedQueries?.find(
    query => query.name === newQueryName
  );
  return (
    <Modal isOpen={isSaveAsModalOpen} onClose={onCloseSaveAsModal}>
      <ModalHeader>Save Query</ModalHeader>
      <ModalBody>
        <FormLabel htmlFor="newQueryName">Query Name</FormLabel>
        <Input
          id="newQueryName"
          onChange={e => {
            setNewQueryName(e.target.value);
          }}
        />
      </ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button colorScheme="gray" onClick={() => onCloseSaveAsModal()}>
            Cancel
          </Button>
          <Button
            isDisabled={newQueryName === ""}
            colorScheme="green"
            onClick={async () => {
              if (queryExists) {
                await onSave(newQueryName);
                setQueryName(newQueryName);
                onCloseSaveAsModal();
                return;
              }
              await onSaveAs(newQueryName);
              setQueryName(newQueryName);
              onCloseSaveAsModal();
            }}
          >
            {queryExists ? "Update" : "Save"}
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
