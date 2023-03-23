import { SettingsIcon } from "@chakra-ui/icons";
import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box,
  Button,
  Flex,
  Menu,
  MenuButton,
  useMenuContext,
  useMenuPositioner,
  useMultiStyleConfig
} from "@chakra-ui/react";
import React, { ReactNode } from "react";
import { AccordionEdgesContent } from "./AccordionEdgesContent";
import { AccordionGraphContent } from "./AccordionGraphContent";
import { AccordionNodesContent } from "./AccordionNodesContent";
import { useGraph } from "./GraphContext";

export const GraphSettings = () => {
  const { onOpenSettings } = useGraph();
  return (
    <Menu closeOnSelect={false} closeOnBlur={false}>
      <MenuButton
        as={Button}
        size="sm"
        leftIcon={<SettingsIcon />}
        aria-label={"Settings"}
        colorScheme="green"
        onClick={onOpenSettings}
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
    <Box
      {...positionerProps}
      __css={{
        ...styles.list
      }}
      maxHeight="600px"
      overflow="auto"
      width="400px"
      position="relative"
      paddingY="0"
    >
      <Accordion allowMultiple allowToggle defaultIndex={[0, 1, 2]}>
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
      <Flex
        background="white"
        borderTop="1px solid"
        borderColor="gray.300"
        position="sticky"
        bottom="0"
        justifyContent="end"
        padding="3"
      >
        <ApplyButton />
      </Flex>
    </Box>
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
        <Box as="span" flex="1" textAlign="left">
          {title}
        </Box>
        <AccordionIcon />
      </AccordionButton>

      <AccordionPanel pb={4}>{children}</AccordionPanel>
    </AccordionItem>
  );
};
const ApplyButton = () => {
  return (
    <Button colorScheme="green" onClick={() => {}}>
      Apply
    </Button>
  );
};
