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
import { useGraphsModeContext } from "../listGraphs/GraphsModeContext";
import { EnterpriseGraphForm } from "./EnterpriseGraphForm";
import { ExampleGraphForm } from "./ExampleGraphForm";
import { GeneralGraphForm } from "./GeneralGraphForm";
import { SatelliteGraphForm } from "./SatelliteGraphForm";
import { SmartGraphForm } from "./SmartGraphForm";

const ALL_GRAPH_TABS = [
  {
    id: "examples",
    label: "Examples",
    Component: ExampleGraphForm
  },
  {
    id: "satellite",
    label: "SatelliteGraph",
    Component: SatelliteGraphForm
  },
  {
    id: "general",
    label: "GeneralGraph",
    Component: GeneralGraphForm
  },
  {
    id: "smart",
    label: "SmartGraph",
    Component: SmartGraphForm
  },
  {
    id: "enterprise",
    label: "EnterpriseGraph",
    Component: EnterpriseGraphForm
  }
];
const COMMUNITY_GRAPH_TYPES = ["examples", "general"];
const COMMUNITY_TABS = ALL_GRAPH_TABS.filter(tab =>
  COMMUNITY_GRAPH_TYPES.includes(tab.id)
);

export const AddGraphModal = ({
  isOpen,
  onClose
}: {
  isOpen: boolean;
  onClose: () => void;
}) => {
  const isEnterprise = window.frontendConfig.isEnterprise;
  const initialFocusRef = React.useRef<HTMLInputElement>(null);
  const { mode } = useGraphsModeContext();
  const defaultIndex = isEnterprise ? 4 : 1;

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
            {mode === "add" ? "Create Graph" : "View Graph"}
          </Heading>
        </Flex>
      </ModalHeader>
      <ModalBody>
        <Tabs size="md" variant="enclosed-colored" defaultIndex={defaultIndex}>
          <TabsInner onClose={onClose} />
        </Tabs>
      </ModalBody>
    </Modal>
  );
};

const TabsInner = ({ onClose }: { onClose: () => void }) => {
  const TABS_LIST = window.frontendConfig.isEnterprise
    ? ALL_GRAPH_TABS
    : COMMUNITY_TABS;
  return (
    <>
      <TabList>
        {TABS_LIST.map(tab => {
          return <Tab key={tab.id}>{tab.label}</Tab>;
        })}
      </TabList>
      <TabPanels>
        {TABS_LIST.map(tab => {
          return (
            <TabPanel key={tab.id}>
              <tab.Component onClose={onClose} />
            </TabPanel>
          );
        })}
      </TabPanels>
    </>
  );
};
