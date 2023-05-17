import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box
} from "@chakra-ui/react";
import React, { ReactNode } from "react";
import { FormDispatch } from "../../utils/constants";
import { FormState } from "./constants";
import ConsolidationPolicyForm from "./forms/ConsolidationPolicyForm";
import { GeneralContent } from "./GeneralContent";
import { LinksContent } from "./LinksContent";
import { getPrimarySortTitle, PrimarySortContent } from "./PrimarySortContent";
import { StoredValuesContent } from "./StoredValuesContent";

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

const ViewConfigForm = ({
  formState,
  dispatch,
  isAdminUser
}: {
  formState: FormState;
  dispatch: FormDispatch<FormState>;
  isAdminUser: boolean;
}) => {
  return (
    <Accordion
      backgroundColor="#f9f9f9"
      allowToggle
      allowMultiple
      defaultIndex={[0]}
    >
      <AccordionItem>
        <AccordionHeader>Links</AccordionHeader>
        <AccordionPanel pb={4}>
          <LinksContent />
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>General</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>
            <GeneralContent
              formState={formState}
              dispatch={dispatch}
              isAdminUser={isAdminUser}
            />
          </div>
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>Consolidation Policy</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>
            {" "}
            <ConsolidationPolicyForm
              formState={formState}
              dispatch={dispatch}
              disabled={!isAdminUser}
            />
          </div>
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>{getPrimarySortTitle({ formState })}</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>
            <PrimarySortContent formState={formState} />
          </div>
        </AccordionPanel>
      </AccordionItem>
      <AccordionItem>
        <AccordionHeader>Stored Values</AccordionHeader>
        <AccordionPanel pb={4}>
          <div>
            <StoredValuesContent formState={formState} />
          </div>
        </AccordionPanel>
      </AccordionItem>
    </Accordion>
  );
};

export const ViewLeftPane = ({
  formState,
  dispatch,
  isAdminUser
}: {
  formState: FormState;
  dispatch: FormDispatch<FormState>;
  isAdminUser: boolean;
}) => {
  return (
    <Box
      minWidth="0"
      paddingY="4"
      display="flex"
      height="full"
      backgroundColor="#eeeee"
    >
      <Box maxW="calc(100% - 48px)" marginX="auto" flexGrow={1}>
        <ViewConfigForm
          formState={formState}
          dispatch={dispatch}
          isAdminUser={isAdminUser}
        />
      </Box>
    </Box>
  );
};
