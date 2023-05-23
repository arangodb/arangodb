import { Checkbox, Grid, HStack, Stack } from "@chakra-ui/react";
import React from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useEditViewContext } from "../../editView/EditViewContext";
import { AnalyzersDropdown } from "./AnalyzersDropdown";
import { FieldsDropdown } from "./FieldsDropdown";
import { LinksBreadCrumb } from "./LinksBreadCrumb";

export const LinksDetails = () => {
  const { currentLink } = useEditViewContext();
  if (!currentLink) {
    return null;
  }
  return (
    <Stack backgroundColor="gray.100">
      <LinksBreadCrumb />
      <Grid templateColumns="minmax(300px, 1fr) 1fr" columnGap="10">
        <Stack padding="2">
          <AnalyzersDropdown />
          <FieldsDropdown />
        </Stack>
        <Stack padding="2">
          <HStack>
            <Checkbox
              margin="0"
              borderColor="gray.400"
              id="includeAllFields"
              onChange={() => {
                console.log("onChange");
              }}
            >
              Include All Fields
            </Checkbox>
            <InfoTooltip label="Process all document attributes." />
          </HStack>
        </Stack>
      </Grid>
    </Stack>
  );
};
