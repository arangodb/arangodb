import {
  Box,
  Button,
  Checkbox,
  ModalBody,
  ModalFooter,
  ModalHeader,
  Stack,
  Text
} from "@chakra-ui/react";
import { AnalyzerDescription } from "arangojs/analyzer";
import React from "react";
import { mutate } from "swr";
import { Modal } from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";

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
  const [shouldForceDelete, setShouldForceDelete] = React.useState(false);
  const onDelete = async () => {
    const currentDB = getCurrentDB();
    setIsLoading(true);
    try {
      await currentDB.analyzer(analyzer.name).drop(shouldForceDelete);
      mutate("/analyzers");
      setIsLoading(false);
      onSuccess();
    } catch (e: any) {
      window.arangoHelper.arangoError(
        "Analyzer",
        `Could not delete analyzer: ${e.response.parsedBody.errorMessage}`
      );
      setIsLoading(false);
    }
  };
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete Analyzer: "{analyzer.name}"?</ModalHeader>
      <ModalBody>
        <Text>
          This analyzer might be in use. Deleting it will impact the Views using
          it. Clicking on the "Delete" button will only delete the analyzer if
          it is not in use.
        </Text>
        <Text>
          If you want to delete it anyway, check the "Force delete" checkbox
          below.
        </Text>
        <Box marginTop="2">
          <Checkbox
            size="sm"
            isChecked={shouldForceDelete}
            onChange={e => setShouldForceDelete(e.target.checked)}
          >
            Force delete
          </Checkbox>
        </Box>
      </ModalBody>
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
