import {
  Flex,
  Heading,
  Tab,
  TabList,
  TabPanel,
  TabPanels,
  Tabs
} from "@chakra-ui/react";
import React from "react";
import { Modal, ModalBody, ModalHeader } from "../../../components/modal";
import { EnterpriseGraphForm } from "./EnterpriseGraphForm";
import { GeneralGraphForm } from "./GeneralGraphForm";

export const AddGraphModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  return (
    <Modal
      initialFocusRef={initialFocusRef}
      size="6xl"
      isOpen={isOpen}
      onClose={onClose}
    >
      <ModalHeader fontSize="sm" fontWeight="normal">
        <Flex direction="row" alignItems="center">
          <Heading marginRight="4" size="md">
            Create Graph
          </Heading>
        </Flex>
      </ModalHeader>

      <AddGraphModalInner initialFocusRef={initialFocusRef} onClose={onClose} />
    </Modal>
  );
};

const AddGraphModalInner = ({
  initialFocusRef,
  onClose
}: {
  initialFocusRef: React.RefObject<HTMLInputElement>;
  onClose: () => void;
}) => {
  return (
    <ModalBody>
      <Tabs size="md" variant="enclosed" defaultIndex={2}>
        <TabList>
          <Tab>Examples</Tab>
          <Tab>SatelliteGraph</Tab>
          <Tab>GeneralGraph</Tab>
          <Tab>SmartGraph</Tab>
          <Tab>EnterpriseGraph</Tab>
        </TabList>
        <TabPanels>
          <TabPanel>
            <p>Examples!</p>
          </TabPanel>
          <TabPanel>
            <p>SatelliteGraph!</p>
          </TabPanel>
          <TabPanel>
            <GeneralGraphForm onClose={onClose} />
          </TabPanel>
          <TabPanel>
            <p>SmartGraph!</p>
          </TabPanel>
          <TabPanel>
            <EnterpriseGraphForm initialFocusRef={initialFocusRef} />
          </TabPanel>
        </TabPanels>
      </Tabs>
    </ModalBody>
  );
};
