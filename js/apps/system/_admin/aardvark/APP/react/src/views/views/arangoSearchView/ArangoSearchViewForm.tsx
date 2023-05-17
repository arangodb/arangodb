import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box
} from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { FormField } from "../../../components/form/FormField";

import { useArangoSearchFieldsData } from "./useArangoSearchFieldsData";

export const ArangoSearchViewForm = () => {
  const { values } = useFormikContext();
  console.log("values", values);
  return (
    <Accordion
      borderColor={"gray.200"}
      defaultIndex={[0, 1]}
      borderRightWidth="1px solid"
      borderLeftWidth="1px solid"
      marginTop="4"
      allowMultiple
      allowToggle
      padding="4"
    >
      <GeneralAccordionItem />
    </Accordion>
  );
};

const FieldsGrid = ({ children }: { children: React.ReactNode }) => {
  return (
    <Box
      display={"grid"}
      gridTemplateColumns={"200px 1fr 40px"}
      rowGap="5"
      columnGap="3"
      maxWidth="800px"
      paddingRight="8"
      paddingLeft="4"
      alignItems="center"
      marginTop="4"
    >
      {children}
    </Box>
  );
};

const GeneralAccordionItem = () => {
  const { fields } = useArangoSearchFieldsData();
  const generalFields = fields.filter(field => field.group === "general");
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          General
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <FieldsGrid>
          {generalFields.map(field => {
            return <FormField field={field} key={field.name} />;
          })}
        </FieldsGrid>
      </AccordionPanel>
    </AccordionItem>
  );
};
