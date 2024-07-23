import { Flex, Heading } from "@chakra-ui/react";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { Modal, ModalBody, ModalHeader } from "../../../components/modal";
import { EnterpriseGraphForm } from "../addGraph/EnterpriseGraphForm";
import { GeneralGraphForm } from "../addGraph/GeneralGraphForm";
import { SatelliteGraphForm } from "../addGraph/SatelliteGraphForm";
import { SmartGraphForm } from "../addGraph/SmartGraphForm";
import { detectGraphType } from "./graphListHelpers";
import { GraphsModeProvider } from "./GraphsModeContext";

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
  const graphType = detectGraphType(graph);
  const GraphComponent = TYPE_TO_COMPONENT_MAP[graphType?.type];
  return (
    <GraphsModeProvider mode="edit" initialGraph={graph}>
      <Modal size="6xl" isOpen={isOpen} onClose={onClose}>
        <ModalHeader fontSize="sm" fontWeight="normal">
          <Flex direction="row" alignItems="center">
            <Heading marginRight="4" size="md">
              Graph: {graph.name}
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
