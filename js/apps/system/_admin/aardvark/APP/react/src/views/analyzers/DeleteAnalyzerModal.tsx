import {
  Button,
  ModalBody,
  ModalFooter,
  ModalHeader,
  Stack
} from "@chakra-ui/react";
import { AnalyzerDescription } from "arangojs/analyzer";
import React from "react";
import { mutate } from "swr";
import { Modal } from "../../components/modal";
import { getCurrentDB } from "../../utils/arangoClient";

export const DeleteAnalyzerModal = ({
  analyzer,
  onClose,
  onSuccess
}: {
  analyzer: AnalyzerDescription;
  onClose: () => void;
  onSuccess: () => void;
}) => {
  const [isLoading, setIsLoading] = React.useState(false);
  const onDelete = async () => {
    const currentDB = getCurrentDB();
    setIsLoading(true);
    await currentDB.analyzer(analyzer.name).drop();
    mutate("/analyzers");
    setIsLoading(false);
    onSuccess();
  };
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete Analyzer</ModalHeader>
      <ModalBody>Are you sure you want to delete {analyzer.name}?</ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button
            isDisabled={isLoading}
            colorScheme="gray"
            onClick={() => onClose()}
          >
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
