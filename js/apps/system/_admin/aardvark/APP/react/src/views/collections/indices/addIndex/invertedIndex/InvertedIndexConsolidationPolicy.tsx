import { Box, FormLabel } from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { IndexFormField, IndexFormFieldProps } from "../IndexFormField";
import { IndexInfoTooltip } from "../IndexInfoTooltip";

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
        <IndexInfoTooltip label="Represents the type of policy." />
        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsMin",
            label: "Segments Min",
            type: "number",
            isDisabled: field.isDisabled,
            tooltip:
              "The minimum number of segments that will be evaluated as candidates for consolidation."
          }}
        />
        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsMax",
            label: "Segments Max",
            type: "number",
            isDisabled: field.isDisabled,
            tooltip:
              "The maximum number of segments that will be evaluated as candidates for consolidation."
          }}
        />
        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsBytesMax",
            label: "Segments Bytes Max",
            type: "number",
            isDisabled: field.isDisabled,
            tooltip:
              "Maximum allowed size of all consolidated segments in bytes."
          }}
        />
        <IndexFormField
          field={{
            name: "consolidationPolicy.segmentsBytesFloor",
            label: "Segments Bytes Floor",
            type: "number",
            isDisabled: field.isDisabled,
            tooltip:
              "Defines the value (in bytes) to treat all smaller segments as equal for consolidation selection."
          }}
        />
      </Box>
    </>
  );
};
