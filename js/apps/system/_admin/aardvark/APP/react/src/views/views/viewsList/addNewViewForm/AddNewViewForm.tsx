import { Accordion, Box, FormLabel } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { InputControl } from "../../../../components/form/InputControl";
import { SelectControl } from "../../../../components/form/SelectControl";
import { AddNewViewFormValues } from "./AddNewViewForm.types";
import { AdvancedAccordionItem } from "./AdvancedAccordionItem";
import { IndexesForm } from "./IndexesForm";
import { PrimarySortAccordionItem } from "./PrimarySortAccordionItem";
import { StoredValuesAccordionItem } from "./StoredValuesAccordionItem";

const typeOptions = [
  {
    label: "arangosearch",
    value: "arangosearch"
  },
  {
    label: "search-alias",
    value: "search-alias"
  }
];

export const AddNewViewForm = () => {
  return (
    <Box>
      <Box display={"grid"} gridTemplateColumns={"200px 1fr"} rowGap="5">
        <FormLabel htmlFor="name">Name</FormLabel>
        <InputControl name="name" />
        <FormLabel htmlFor="type">Type</FormLabel>
        <SelectControl
          name="type"
          selectProps={{
            options: typeOptions
          }}
        />
      </Box>
      <ViewTypeForm />
    </Box>
  );
};

const ViewTypeForm = () => {
  const { values } = useFormikContext<AddNewViewFormValues>();
  if (values.type === "arangosearch") {
    return (
      <Box marginTop="5">
        <Accordion
          borderColor={"gray.200"}
          borderRightWidth="1px solid"
          borderLeftWidth="1px solid"
          marginTop="4"
          allowMultiple
          allowToggle
        >
          <PrimarySortAccordionItem />
          <StoredValuesAccordionItem />
          <AdvancedAccordionItem />
        </Accordion>
      </Box>
    );
  }
  return <IndexesForm />;
};
