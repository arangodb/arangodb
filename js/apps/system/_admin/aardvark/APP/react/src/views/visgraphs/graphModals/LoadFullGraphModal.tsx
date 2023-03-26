import { Button, HStack } from "@chakra-ui/react";
import React, { useContext } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { useGraph } from "../GraphContext";
import { UrlParametersContext } from "../url-parameters-context";

export const LoadFullGraphModal = () => {
  const { onClearAction, onApplySettings, setSelectedAction } = useGraph();
  const [_, setUrlParameters] = useContext(UrlParametersContext) || [];

  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Fetch full graph</ModalHeader>
      <ModalBody>
        Caution: Really load full graph? If no limit is set, your result set
        could be too big.
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
