import { Button, HStack, Text } from "@chakra-ui/react";
import React from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../../components/modal";
import { useGraph } from "../GraphContext";
import {
  DEFAULT_URL_PARAMETERS,
  useUrlParameterContext
} from "../UrlParametersContext";

export const LoadFullGraphModal = () => {
  const { onClearAction, onApplySettings, setSelectedAction } = useGraph();
  const { urlParams } = useUrlParameterContext();
  const curentLimit = Number(urlParams.limit);
  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Are you sure?</ModalHeader>
      <ModalBody>
        <Text color="red.600" fontWeight="semibold" fontSize="lg">
          Caution: Do you want to load the entire graph?
        </Text>
        <Text>If no limit is set, your result set could be too big.</Text>
        <Text>
          The default limit is set to {DEFAULT_URL_PARAMETERS.limit} nodes.
        </Text>
        <Text>
          Current limit: {curentLimit ? `${urlParams.limit} nodes` : "Not set"}
        </Text>
      </ModalBody>
      <ModalFooter>
        <HStack>
          <Button onClick={onClearAction}>Cancel</Button>
          <Button
            colorScheme="green"
            onClick={() => {
              onApplySettings({ mode: "all" });
              setSelectedAction(undefined);
            }}
          >
            Load Full Graph
          </Button>
        </HStack>
      </ModalFooter>
    </Modal>
  );
};
