import { Button, HStack, Text } from "@chakra-ui/react";
import React, { useContext } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useGraph } from "../GraphContext";
import { UrlParametersContext } from "../url-parameters-context";
import URLPARAMETERS from "../UrlParameters";

export const LoadFullGraphModal = () => {
  const { onClearAction, onApplySettings, setSelectedAction } = useGraph();
  const [urlParameters, setUrlParameters] =
    useContext(UrlParametersContext) || [];
  const curentLimit = Number((urlParameters as any).limit);
  console.log({ curentLimit });
  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Load full graph?</ModalHeader>
      <ModalBody>
        <Text color="red.600" fontWeight="semibold" fontSize="lg">
          Caution: Really load the full graph?
        </Text>
        <Text>If no limit is set, your result set could be too big.</Text>
        <Text>The default limit is set to {URLPARAMETERS.limit} nodes.</Text>
        <Text>
          Current limit:{" "}
          {curentLimit ? `${(urlParameters as any).limit} nodes` : "Not set"}
        </Text>
      </ModalBody>
      <ModalFooter>
        <HStack>
          <Button onClick={onClearAction}>Cancel</Button>
          <Button
            colorScheme="green"
            onClick={() => {
              (setUrlParameters as any)?.((urlParameters: any) => {
                return { ...urlParameters, mode: "all" };
              });
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
