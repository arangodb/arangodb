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
import { ExampleGraphForm } from "./ExampleGraphForm";
import { GeneralGraphForm } from "./GeneralGraphForm";
import { SatelliteGraphForm } from "./SatelliteGraphForm";
import { SmartGraphForm } from "./SmartGraphForm";

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

      <AddGraphModalInner onClose={onClose} />
    </Modal>
  );
};

const AddGraphModalInner = ({ onClose }: { onClose: () => void }) => {
  const defaultIndex = window.frontendConfig.isEnterprise ? 4 : 0;
  return (
    <ModalBody>
      <Tabs size="md" variant="enclosed-colored" defaultIndex={defaultIndex}>
        <TabList>
          <Tab>Examples</Tab>
          {window.frontendConfig.isEnterprise && <Tab>SatelliteGraph</Tab>}
          <Tab>GeneralGraph</Tab>
          {window.frontendConfig.isEnterprise && <Tab>SmartGraph</Tab>}
          {window.frontendConfig.isEnterprise && <Tab>EnterpriseGraph</Tab>}
        </TabList>
        <TabPanels>
          <TabPanel>
            <ExampleGraphForm onClose={onClose} />
          </TabPanel>
          {window.frontendConfig.isEnterprise && (
            <TabPanel>
              <SatelliteGraphForm onClose={onClose} />
            </TabPanel>
          )}
          <TabPanel>
            <GeneralGraphForm onClose={onClose} />
          </TabPanel>
          {window.frontendConfig.isEnterprise && (
            <TabPanel>
              <SmartGraphForm onClose={onClose} />
            </TabPanel>
          )}
          {window.frontendConfig.isEnterprise && (
            <TabPanel>
              <EnterpriseGraphForm onClose={onClose} />
            </TabPanel>
          )}
        </TabPanels>
      </Tabs>
    </ModalBody>
  );
};
