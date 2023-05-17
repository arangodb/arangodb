import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box
} from "@chakra-ui/react";
import React from "react";
import { FormFieldProps } from "../FormField";
import { IndexFormFieldsList } from "../IndexFormFieldList";
import { InvertedIndexAnalyzerDropdown } from "./InvertedIndexAnalyzerDropdown";
import { InvertedIndexConsolidationPolicy } from "./InvertedIndexConsolidationPolicy";
import { InvertedIndexFieldsDataEntry } from "./InvertedIndexFieldsDataEntry";
import { InvertedIndexPrimarySort } from "./InvertedIndexPrimarySort";
import { InvertedIndexStoredValues } from "./InvertedIndexStoredValues";

export const InvertedIndexLeftPane = ({
  isFormDisabled,
  fields
}: {
  isFormDisabled: boolean | undefined;
  fields: FormFieldProps[];
}) => {
  return (
    <Box
      minHeight={isFormDisabled ? "100vh" : "calc(100vh - 300px)"}
      minWidth="0"
    >
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
        <FieldsAccordionItem isFormDisabled={isFormDisabled} fields={fields} />
        <GeneralAccordionItem isFormDisabled={isFormDisabled} fields={fields} />
        <PrimarySortAccordionItem
          isFormDisabled={isFormDisabled}
          fields={fields}
        />
        <StoredValuesAccordionItem
          isFormDisabled={isFormDisabled}
          fields={fields}
        />
        <ConsolidationPolicyAccordionItem
          isFormDisabled={isFormDisabled}
          fields={fields}
        />
      </Accordion>
    </Box>
  );
};
type AccordionItemType = {
  fields: FormFieldProps[];
  isFormDisabled?: boolean;
};
const GeneralAccordionItem = ({
  fields,
  isFormDisabled
}: AccordionItemType) => {
  const filteredFields = fields.filter(field => field.group === "general");
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          General
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <IndexFormFieldsList
          isFormDisabled={isFormDisabled}
          fields={filteredFields}
          renderField={({ field, autoFocus }) => {
            if (field.name === "fields") {
              return (
                <InvertedIndexFieldsDataEntry
                  field={field}
                  autoFocus={autoFocus}
                />
              );
            }

            return <>{field.name}</>;
          }}
        />
      </AccordionPanel>
    </AccordionItem>
  );
};
const FieldsAccordionItem = ({ fields, isFormDisabled }: AccordionItemType) => {
  const filteredFields = fields.filter(field => field.group === "fields");
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Fields
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <IndexFormFieldsList
          isFormDisabled={isFormDisabled}
          fields={filteredFields}
          renderField={({ field, autoFocus }) => {
            if (field.name === "fields") {
              return (
                <InvertedIndexFieldsDataEntry
                  field={field}
                  autoFocus={autoFocus}
                />
              );
            }
            if (field.name === "analyzer") {
              return (
                <InvertedIndexAnalyzerDropdown
                  field={field}
                  autoFocus={autoFocus}
                />
              );
            }

            return <>{field.name}</>;
          }}
        />
      </AccordionPanel>
    </AccordionItem>
  );
};
const PrimarySortAccordionItem = ({
  fields,
  isFormDisabled
}: AccordionItemType) => {
  const filteredFields = fields.filter(field => field.name === "primarySort");
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Primary Sort
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <IndexFormFieldsList
          isFormDisabled={isFormDisabled}
          fields={filteredFields}
          renderField={({ field }) => {
            if (field.name === "primarySort") {
              return <InvertedIndexPrimarySort field={field} />;
            }

            return <>{field.name}</>;
          }}
        />
      </AccordionPanel>
    </AccordionItem>
  );
};
const StoredValuesAccordionItem = ({
  fields,
  isFormDisabled
}: AccordionItemType) => {
  const filteredFields = fields.filter(field => field.name === "storedValues");
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Stored Values
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <IndexFormFieldsList
          isFormDisabled={isFormDisabled}
          fields={filteredFields}
          renderField={({ field }) => {
            if (field.name === "storedValues") {
              return <InvertedIndexStoredValues field={field} />;
            }

            return <>{field.name}</>;
          }}
        />
      </AccordionPanel>
    </AccordionItem>
  );
};
const ConsolidationPolicyAccordionItem = ({
  fields,
  isFormDisabled
}: AccordionItemType) => {
  const filteredFields = fields.filter(
    field => field.name === "consolidationPolicy"
  );
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Consolidation Policy
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <IndexFormFieldsList
          isFormDisabled={isFormDisabled}
          fields={filteredFields}
          renderField={({ field }) => {
            if (field.name === "consolidationPolicy") {
              return <InvertedIndexConsolidationPolicy field={field} />;
            }
            return <>{field.name}</>;
          }}
        />
      </AccordionPanel>
    </AccordionItem>
  );
};
