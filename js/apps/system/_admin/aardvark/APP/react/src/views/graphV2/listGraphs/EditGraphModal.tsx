import { Modal, ModalBody, ModalHeader } from "../../../components/modal";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { GraphsModeProvider } from "../GraphsModeContext";
import { Flex, Heading } from "@chakra-ui/react";
import { SatelliteGraphForm } from "../addGraph/SatelliteGraphForm";
import { SmartGraphForm } from "../addGraph/SmartGraphForm";
import { EnterpriseGraphForm } from "../addGraph/EnterpriseGraphForm";
import { GeneralGraphForm } from "../addGraph/GeneralGraphForm";
import { detectType } from "../GraphsHelpers";

const TYPE_TO_COMPONENT_MAP = {
  general: GeneralGraphForm,
  enterprise: EnterpriseGraphForm,
  satellite: SatelliteGraphForm,
  smart: SmartGraphForm
};

export const EditGraphModal = ({
  graph,
  isOpen,
  onClose
}: {
  graph: GraphInfo;
  isOpen: boolean;
  onClose: () => void;
}) => {
  const graphType = detectType(graph);
  const GraphComponent = TYPE_TO_COMPONENT_MAP[graphType?.type];
  return (
    <GraphsModeProvider mode="edit" initialGraph={graph}>
      <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
        <ModalHeader fontSize="sm" fontWeight="normal">
          <Flex direction="row" alignItems="center">
            <Heading marginRight="4" size="md">
              Edit graph
            </Heading>
          </Flex>
        </ModalHeader>
        <ModalBody>
          <GraphComponent onClose={onClose} />
        </ModalBody>
      </Modal>
    </GraphsModeProvider>
  );
};
