import {
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box,
  FormLabel
} from "@chakra-ui/react";
import React from "react";
import { CreatableMultiSelectControl } from "../../../../components/form/CreatableMultiSelectControl";
import { InputControl } from "../../../../components/form/InputControl";
import { SwitchControl } from "../../../../components/form/SwitchControl";

export const AdvancedAccordionItem = () => {
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Advanced
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <AdvancedFields />
      </AccordionPanel>
    </AccordionItem>
  );
};
const AdvancedFields = () => {
  return (
    <Box display={"grid"} gridTemplateColumns={"200px 1fr"} rowGap="5">
      <FormLabel htmlFor="primaryKeyCache">Primary Key Cache</FormLabel>
      <SwitchControl
        name="primaryKeyCache"
        switchProps={{
          isDisabled: !window.frontendConfig.isEnterprise
        }}
        tooltip={
          window.frontendConfig.isEnterprise
          ? undefined
          : "Field normalization value caching is available in Enterprise plans."
        }
      />
      <FormLabel htmlFor="writebufferIdle">Write Buffer Idle</FormLabel>
      <InputControl inputProps={{
        type: 'number'
      }} name="writebufferIdle" />
      <FormLabel htmlFor="writebufferActive">Write Buffer Active</FormLabel>
      <InputControl inputProps={{
        type: 'number'
      }} name="writebufferActive" />
      <FormLabel htmlFor="writebufferSizeMax">Write Buffer Size Max</FormLabel>
      <InputControl inputProps={{
        type: 'number'
      }} name="writebufferSizeMax" />
      <FormLabel htmlFor="optimizeTopK">Optimize Top K</FormLabel>
      <CreatableMultiSelectControl name="optimizeTopK" />
    </Box>
  );
};
