import { Button, Input, Stack, Text } from "@chakra-ui/react";
import React, { useState } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useQueryImport } from "./useQueryImport";

export const ImportQueryModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const formatText = `JSON documents embedded into a list:
      [{
        "name": "Query Name",
        "value": "Query Definition",
        "parameter": "Query Bind Parameter as Object"
      }]
    `;
  const [file, setFile] = useState<File | undefined>();
  const onSelectFile = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) {
      return;
    }
    setFile(file);
  };
  const { onUpload, isUploading } = useQueryImport({ onClose });
  return (
    <Modal size="2xl" isOpen={isOpen} onClose={onClose}>
      <ModalHeader>Import custom queries</ModalHeader>
      <ModalBody>
        <Stack>
          <Text fontWeight="medium" fontSize="lg">
            Format:
          </Text>
          <Text as="pre">{formatText}</Text>
        </Stack>
        <Stack>
          <Text fontWeight="medium" fontSize="lg">
            File:
          </Text>
          <Input
            type="file"
            onChange={onSelectFile}
            padding="2"
            _focus={{
              outlineColor: "var(--chakra-colors-blue-400) !important"
            }}
            accept=".json"
            sx={{
              height: "48px !important",
              "::-webkit-file-upload-button": {
                borderRadius: "md",
                border: 0,
                background: "gray.200"
              },
              "::-webkit-file-upload-button:hover": {
                cursor: "pointer",
                background: "gray.300"
              }
            }}
          />
        </Stack>
      </ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button colorScheme="gray" onClick={onClose}>
            Cancel
          </Button>
          <Button
            isLoading={isUploading}
            colorScheme="green"
            onClick={() => onUpload(file)}
            isDisabled={!file}
          >
            Import
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
