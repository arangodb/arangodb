/* global frontendConfig */

import React, { ReactNode } from "react";
import "./split-pane-styles.css";
import "./viewsheader.css";

import {
  Accordion,
  AccordionItem,
  AccordionButton,
  AccordionPanel,
  AccordionIcon,
  Box
} from "@chakra-ui/react";
import { LinksContent } from "./LinksContent";

export const ViewSettings = ({ name }: { name: string }) => {
  return (
    <div>
      <Header name={name} />
      <div>
        <ViewConfigForm name={name} />
      </div>
      <div>body right</div>
    </div>
  );
};

const AccordionHeader = ({ children }: { children: ReactNode }) => {
  return (
    <AccordionButton>
      <Box as="span" flex="1" textAlign="left">
        {children}
      </Box>
      <AccordionIcon />
    </AccordionButton>
  );
};

const ViewConfigForm = ({ name }: { name: string }) => {
  return (
    <Accordion allowToggle allowMultiple>
      <AccordionItem>
        <AccordionHeader>Links</AccordionHeader>
        <AccordionPanel pb={4}>
          <LinksContent name={name} />
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>General</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>GeneralContent</div>
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>Consolidation Policy</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>Consolidation PolicyContent</div>
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>Primary Sort</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>Primary SortContent</div>
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>Stored Values</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>Stored ValuesContent</div>
        </AccordionPanel>
      </AccordionItem>
    </Accordion>
  );
};

function Header({ name }: { name: string }) {
  return (
    <div>
      <div>{name}</div>
      Copy mutable properties
    </div>
  );
}
