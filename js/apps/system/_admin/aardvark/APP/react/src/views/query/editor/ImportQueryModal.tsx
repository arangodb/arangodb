import { Button, Input, Stack, Text } from "@chakra-ui/react";
import React, { useState } from "react";
import { mutate } from "swr";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { getAardvarkRouteForCurrentDb } from "../../../utils/arangoClient";
import { useQueryContext } from "../QueryContextProvider";

export const ImportQueryModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const { onSaveQueryList } = useQueryContext();
  const formatText = `JSON documents embedded into a list:
      [{
        "name": "Query Name",
        "value": "Query Definition",
        "parameter": "Query Bind Parameter as Object"
      }]
    `;
  const [isUploading, setIsUploading] = useState(false);
  const [file, setFile] = useState<File | null>(null);
  const onSelectFile = (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    setFile(file);
  };
  const onUpload = () => {
    if (!file) return;
    if (window.frontendConfig.ldapEnabled) {
      setIsUploading(true);
      const reader = new FileReader();
      reader.readAsText(file);
      reader.onload = async () => {
        const data = reader.result;
        if (typeof data !== "string") return;
        const queries = JSON.parse(data);
        await onSaveQueryList(queries);
        await mutate("/savedQueries");
        setIsUploading(false);
        onClose();
      };
      return;
    }
    const route = getAardvarkRouteForCurrentDb(
      `query/upload/${window.App.currentUser}`
    );
    setIsUploading(true);
    try {
      const reader = new FileReader();
      reader.readAsText(file);
      reader.onload = async () => {
        const data = reader.result;
        if (typeof data !== "string") return;
        const queries = JSON.parse(data);
        await route.request({
          method: "POST",
          body: queries
        });
        await mutate("/savedQueries");
        window.arangoHelper.arangoNotification("Successfully uploaded queries");
        setIsUploading(false);
        onClose();
      };
    } catch (e) {
      window.arangoHelper.arangoError("Failed to upload queries");
      setIsUploading(false);
      onClose();
    }
  };
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
            onClick={onUpload}
            isDisabled={!file}
          >
            Import
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
  );
};
