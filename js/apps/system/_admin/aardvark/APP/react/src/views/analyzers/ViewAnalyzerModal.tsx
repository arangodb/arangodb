import { Button, Text } from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../components/modal";
import { useAnalyzersContext } from "./AnalyzersContext";

export const ViewAnalyzerModal = () => {
  const { viewAnalyzerName, setViewAnalyzerName } = useAnalyzersContext();
  if (!viewAnalyzerName) {
    return null;
  }
  return (
    <Modal
      size="6xl"
      isOpen
      onClose={() => {
        setViewAnalyzerName("");
      }}
    >
      <ModalHeader>
        <Text>{viewAnalyzerName}</Text>
      </ModalHeader>
      <ModalBody>todo</ModalBody>
      <ModalFooter>
        <Button
          colorScheme={"gray"}
          onClick={() => {
            setViewAnalyzerName("");
          }}
        >
          Close
        </Button>
      </ModalFooter>
    </Modal>
  );
};
