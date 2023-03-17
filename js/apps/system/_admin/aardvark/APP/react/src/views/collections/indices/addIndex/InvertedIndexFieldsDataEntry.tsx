import { Box } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import React from "react";
import { FieldBreadcrumbs } from "./FieldBreadcrumbs";
import { FieldsDropdown } from "./FieldsDropdown";
import { IndexFormField, IndexFormFieldProps } from "./IndexFormField";
import { InvertedIndexAnalyzerDropdown } from "./InvertedIndexAnalyzerDropdown";
import { FieldData, useInvertedIndexContext } from "./InvertedIndexContext";
import {
  invertedIndexFieldsMap,
  InvertedIndexValuesType
} from "./useCreateInvertedIndex";

const SelectedFieldDetails = ({
  currentFieldData
}: {
  currentFieldData?: FieldData;
}) => {
  const fullPath =
    currentFieldData &&
    `${currentFieldData.fieldName}[${currentFieldData.fieldIndex}]`;
  const { values } = useFormikContext<InvertedIndexValuesType[]>();
  if (!currentFieldData || !fullPath) {
    return null;
  }
  return (
    <Box>
      <FieldBreadcrumbs values={values} fullPath={fullPath} />
      <Box padding="4">
        <Box
          display="grid"
          gridTemplateColumns="200px 1fr 40px"
          columnGap="3"
          rowGap="3"
          maxWidth="800px"
          alignItems="center"
        >
          <InvertedIndexAnalyzerDropdown
            autoFocus
            field={{
              ...invertedIndexFieldsMap.analyzer,
              name: `${fullPath}.analyzer`
            }}
            dependentFieldName={`${fullPath}.features`}
          />
          <IndexFormField
            field={{
              ...invertedIndexFieldsMap.features,
              name: `${fullPath}.features`
            }}
          />
          <IndexFormField
            field={{
              ...invertedIndexFieldsMap.includeAllFields,
              name: `${fullPath}.includeAllFields`
            }}
          />
          <IndexFormField
            field={{
              ...invertedIndexFieldsMap.trackListPositions,
              name: `${fullPath}.trackListPositions`
            }}
          />
          <IndexFormField
            field={{
              ...invertedIndexFieldsMap.searchField,
              name: `${fullPath}.searchField`
            }}
          />
          <Box gridColumn="1 / span 3">
            <IndexFormField
              render={({ field }) => {
                return <FieldsDropdown field={field} />;
              }}
              field={{
                ...invertedIndexFieldsMap.fields,
                isRequired: false,
                tooltip: "Nested fields",
                label: "Nested fields",
                name: `${fullPath}.nested`
              }}
            />
          </Box>
        </Box>
      </Box>
    </Box>
  );
};

export const InvertedIndexFieldsDataEntry = ({
  field
}: {
  autoFocus: boolean;
  field: IndexFormFieldProps;
}) => {
  const { currentFieldData } = useInvertedIndexContext();
  return (
    <Box
      gridColumn="1 / span 3"
      border="2px solid"
      borderRadius="md"
      borderColor="gray.200"
    >
      <FieldsDropdown field={field} />
      <SelectedFieldDetails currentFieldData={currentFieldData} />
    </Box>
  );
};
