import { SettingsIcon } from "@chakra-ui/icons";
import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box,
  Button,
  HStack,
  Menu,
  MenuButton,
  Portal,
  useMenuContext,
  useMenuPositioner,
  useMultiStyleConfig
} from "@chakra-ui/react";
import React, { ReactNode } from "react";
import { useGraph } from "../GraphContext";
import { AccordionEdgesContent } from "./AccordionEdgesContent";
import { AccordionGraphContent } from "./AccordionGraphContent";
import { AccordionNodesContent } from "./AccordionNodesContent";

export const GraphSettings = () => {
  return (
    <Menu closeOnSelect={false} closeOnBlur={false}>
      <MenuButton
        as={Button}
        size="sm"
        leftIcon={<SettingsIcon />}
        aria-label={"Settings"}
        colorScheme="green"
      >
        Settings
      </MenuButton>
      <SettingsMenuContent />
    </Menu>
  );
};
const SettingsMenuContent = () => {
  const { isOpen } = useMenuContext();
  const positionerProps = useMenuPositioner();
  const styles = useMultiStyleConfig("Menu", {});

  if (!isOpen) {
    return null;
  }
  return (
    <Portal>
      <Box
        {...positionerProps}
        __css={{
          ...styles.list
        }}
        maxHeight="600px"
        overflow="overlay"
        width="400px"
        position="relative"
        paddingY="0"
      >
        <Accordion allowMultiple allowToggle defaultIndex={[0]}>
          <GraphAccordionItem title="Graph">
            <AccordionGraphContent />
          </GraphAccordionItem>
          <GraphAccordionItem title="Nodes">
            <AccordionNodesContent />
          </GraphAccordionItem>
          <GraphAccordionItem title="Edges">
            <AccordionEdgesContent />
          </GraphAccordionItem>
        </Accordion>
        <HStack
          background="white"
          borderTop="1px solid"
          borderColor="gray.300"
          position="sticky"
          bottom="0"
          justifyContent="end"
          padding="3"
          paddingRight="5"
        >
          <RestoreDefaultsButton />
          <ApplyButton />
        </HStack>
      </Box>
    </Portal>
  );
};
const GraphAccordionItem = ({
  children,
  title
}: {
  children: ReactNode;
  title: string;
}) => {
  return (
    <AccordionItem borderColor="gray.300">
      <AccordionButton>
        <Box as="span" flex="1" textAlign="left" fontWeight="bold">
          {title}
        </Box>
        <AccordionIcon />
      </AccordionButton>

      <AccordionPanel pb={4}>{children}</AccordionPanel>
    </AccordionItem>
  );
};
const RestoreDefaultsButton = () => {
  const { onRestoreDefaults } = useGraph();
  return (
    <Button
      colorScheme="gray"
      variant="ghost"
      onClick={() => onRestoreDefaults()}
    >
      Restore defaults
    </Button>
  );
};
const ApplyButton = () => {
  const { onApplySettings } = useGraph();
  return (
    <Button colorScheme="green" onClick={() => onApplySettings()}>
      Apply
    </Button>
  );
};
