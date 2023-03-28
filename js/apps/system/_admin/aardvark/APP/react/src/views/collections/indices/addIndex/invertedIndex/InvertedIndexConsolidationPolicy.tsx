import { Box, FormLabel, Spacer } from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { IndexFormField, IndexFormFieldProps } from "../IndexFormField";

//
export const InvertedIndexConsolidationPolicy = ({
  field
}: {
  field: IndexFormFieldProps;
}) => {
  return (
    <Box
      gridColumn="1 / span 3"
      border="2px solid"
      borderRadius="md"
      borderColor="gray.200"
      padding="4"
      marginX="-4"
    >
      <ConsolidationPolicyFields field={field} />
    </Box>
  );
};

const ConsolidationPolicyFields = ({
  field
}: {
  field: IndexFormFieldProps;
}) => {
  const [formikField] = useField(field.name);
  return (
    <>
      <Box
        display={"grid"}
        gridColumnGap="4"
        gridTemplateColumns="200px 1fr 40px"
        rowGap="5"
        alignItems={"center"}
        marginTop="2"
      >
        <FormLabel>Type</FormLabel>
        <Box>{formikField.value.type}</Box>
        <Spacer />

        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsBytesFloor",
            label: "Segments Bytes Floor",
            type: "number",
            isDisabled: field.isDisabled
          }}
        />

        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsBytesMax",
            label: "Segments Bytes Max",
            type: "number",
            isDisabled: field.isDisabled
          }}
        />

        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsMin",
            label: "Segments Min",
            type: "number",
            isDisabled: field.isDisabled
          }}
        />
        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsMax",
            label: "Segments Max",
            type: "number",
            isDisabled: field.isDisabled
          }}
        />
      </Box>
    </>
  );
};
