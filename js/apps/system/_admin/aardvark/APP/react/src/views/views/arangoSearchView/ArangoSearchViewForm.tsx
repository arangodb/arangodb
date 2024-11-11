import {
  Accordion,
  AccordionButton,
  AccordionIcon,
  AccordionItem,
  AccordionPanel,
  Box,
  FormLabel,
  Spacer,
  Stack,
  Tag
} from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { FieldsGrid } from "../../../components/form/FieldsGrid";
import { FormField } from "../../../components/form/FormField";
import { IndexInfoTooltip } from "../../collections/indices/addIndex/IndexInfoTooltip";
import { useEditViewContext } from "../editView/EditViewContext";
import { PrimarySortType, StoredValueType } from "../View.types";
import { LinksEditor } from "./linksEditor/LinksEditor";

import { useArangoSearchFieldsData } from "./useArangoSearchFieldsData";

export const ArangoSearchViewForm = () => {
  return (
    <Accordion
      borderColor={"gray.200"}
      defaultIndex={[0]}
      borderRightWidth="1px solid"
      borderLeftWidth="1px solid"
      allowMultiple
      allowToggle
      paddingX="4"
      paddingTop="4"
    >
      <LinksAccordionItem />
      <GeneralAccordionItem />
      <ConsolidationPolicyAccordionItem />
      <PrimarySortAccordionItem />
      <StoredValuesAccordionItem />
    </Accordion>
  );
};

const LinksAccordionItem = () => {
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Links
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <LinksEditor />
      </AccordionPanel>
    </AccordionItem>
  );
};
const GeneralAccordionItem = () => {
  const { fields } = useArangoSearchFieldsData();
  const generalFields = fields.filter(field => field.group === "general");
  const { isAdminUser } = useEditViewContext();

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
            return (
              <FormField
                field={{
                  ...field,
                  isDisabled: field.isDisabled || !isAdminUser
                }}
                key={field.name}
              />
            );
          })}
        </FieldsGrid>
      </AccordionPanel>
    </AccordionItem>
  );
};

const ConsolidationPolicyAccordionItem = () => {
  const { tierConsolidationPolicyFields, bytesAccumConsolidationPolicyFields } =
    useArangoSearchFieldsData();
  const [policyTypeField] = useField("consolidationPolicy.type");
  const { isAdminUser } = useEditViewContext();
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Consolidation Policy
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        <FieldsGrid>
          <FormLabel>Policy Type</FormLabel>
          <Box>{policyTypeField.value}</Box>
          <IndexInfoTooltip label="Represents the type of policy." />
          {policyTypeField.value === "tier"
            ? tierConsolidationPolicyFields.map(field => {
                return (
                  <FormField
                    field={{ ...field, isDisabled: !isAdminUser }}
                    key={field.name}
                  />
                );
              })
            : null}
          {policyTypeField.value === "bytes_accum"
            ? bytesAccumConsolidationPolicyFields.map(field => {
                return (
                  <FormField
                    field={{ ...field, isDisabled: !isAdminUser }}
                    key={field.name}
                  />
                );
              })
            : null}
        </FieldsGrid>
      </AccordionPanel>
    </AccordionItem>
  );
};

const PrimarySortAccordionItem = () => {
  const [primarySortField] = useField<PrimarySortType[] | undefined>(
    "primarySort"
  );
  const [primarySortCacheField] = useField("primarySortCache");
  const [primarySortCompressionField] = useField("primarySortCompression");
  const isPrimarySortEmpty =
    primarySortField.value?.length === 0 || !primarySortField.value;

  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Primary Sort (compression: {primarySortCompressionField.value}{primarySortCacheField.value ? ", cached" : ""})
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        {isPrimarySortEmpty ? (
          <Box padding="4">No fields set</Box>
        ) : (
          <FieldsGrid>
            {primarySortField.value?.map((item: any, index: number) => {
              return (
                <React.Fragment key={`${item.field}_${index}`}>
                  <Box>
                    <Tag>{item.field}</Tag>
                  </Box>
                  <Box>{item.asc ? "asc" : "desc"}</Box>
                  <Spacer />
                </React.Fragment>
              );
            })}
          </FieldsGrid>
        )}
      </AccordionPanel>
    </AccordionItem>
  );
};

const StoredValuesAccordionItem = () => {
  const [storedValuesField] = useField<StoredValueType[] | undefined>(
    "storedValues"
  );
  const isStoredValuesEmpty =
    storedValuesField.value?.length === 0 || !storedValuesField.value;
  return (
    <AccordionItem>
      <AccordionButton>
        <Box flex="1" textAlign="left">
          Stored Values
        </Box>
        <AccordionIcon />
      </AccordionButton>
      <AccordionPanel pb={4}>
        {" "}
        {isStoredValuesEmpty ? (
          <Box padding="4">No fields set</Box>
        ) : (
          <FieldsGrid alignItems="start">
            {storedValuesField.value?.map((item: any, index: number) => {
              return (
                <React.Fragment key={index}>
                  <Stack direction="row" flexWrap="wrap">
                    {item.fields.map((field: any) => {
                      return <Tag key={field}>{field}</Tag>;
                    })}
                  </Stack>
                  <Box>compression: {item.compression}{item.cache && ", cached"}</Box>
                  <Spacer />
                </React.Fragment>
              );
            })}
          </FieldsGrid>
        )}
      </AccordionPanel>
    </AccordionItem>
  );
};
