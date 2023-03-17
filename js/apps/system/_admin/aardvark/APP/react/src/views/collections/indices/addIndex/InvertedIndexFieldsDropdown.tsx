import { Box, FormLabel } from "@chakra-ui/react";
import { FieldArray, useField } from "formik";
import React from "react";
import { components, MultiValueGenericProps } from "react-select";
import {
  CreatableSelectBase,
  OptionType
} from "../../../../components/select/SelectBase";
import { IndexFormField, IndexFormFieldProps } from "./IndexFormField";
import { InvertedIndexAnalyzerDropdown } from "./InvertedIndexAnalyzerDropdown";
import { FieldData, useInvertedIndexContext } from "./InvertedIndexContext";
import {
  invertedIndexFieldsMap,
  InvertedIndexFieldType
} from "./useCreateInvertedIndex";

export const MultiValueLabel = (
  props: MultiValueGenericProps<OptionType> & {
    fieldName: string;
    fieldIndex: number;
  }
) => {
  const { setCurrentFieldData } = useInvertedIndexContext();
  const { fieldName, fieldIndex, ...rest } = props;
  // const fullFieldPath = `${field.name}[${index}]`;

  return (
    <Box
      onClick={() => {
        const fieldData = {
          fieldValue: props.data.value,
          fieldName,
          fieldIndex
        };
        setCurrentFieldData(fieldData);
      }}
      textDecoration="underline"
      cursor="pointer"
    >
      <components.MultiValueLabel {...rest} />
    </Box>
  );
};

export const InvertedIndexFieldsDropdownWrap = ({
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

const SelectedFieldDetails = ({
  currentFieldData
}: {
  currentFieldData?: FieldData;
}) => {
  if (!currentFieldData) {
    return null;
  }
  const fullPath = `${currentFieldData.fieldName}[${currentFieldData.fieldIndex}]`;
  return (
    <Box>
      <Box>
        <Box>Selected Name: {currentFieldData.fieldValue}</Box>
        <Box>
          Selected Path: {currentFieldData.fieldName} -{" "}
          {currentFieldData.fieldIndex}
        </Box>
      </Box>
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
const FieldsDropdown = ({ field }: { field: IndexFormFieldProps }) => {
  const [formikField] = useField<InvertedIndexFieldType[]>(field.name);
  const { setCurrentFieldData } = useInvertedIndexContext();
  const dropdownValue = formikField.value?.map(data => {
    return { label: data.name, value: data.name };
  }) || [];
  return (
    <FieldArray name={field.name}>
      {({ push, remove }) => {
        return (
          <Box
            display="grid"
            gridTemplateColumns="200px 1fr 40px"
            columnGap="3"
            alignItems="center"
          >
            <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
            <CreatableSelectBase
              isMulti
              components={{
                MultiValueLabel: props => {
                  const index = formikField.value?.findIndex(
                    fieldObj => fieldObj.name === props.data.value
                  );
                  return (
                    <MultiValueLabel
                      fieldName={field.name}
                      fieldIndex={index}
                      {...props}
                    />
                  );
                }
              }}
              noOptionsMessage={() => "Start typing to add a field"}
              onChange={(_, action) => {
                if (action.action === "create-option") {
                  push({ name: action.option.value });
                }
                if (action.action === "remove-value") {
                  const index = formikField.value?.findIndex(
                    fieldObj => fieldObj.name === action.removedValue.value
                  );
                  if (index > -1) {
                    remove(index);
                    // const fieldNameParts = field.name.split(".");
                    // const currentFieldNameParts = currentFieldData?.fieldName.split(
                    //   "."
                    // );
                    // if(fieldNameParts[fieldNameParts.length - 1] === currentFieldNameParts[])
                    setCurrentFieldData(undefined);
                  }
                }
              }}
              value={dropdownValue}
            ></CreatableSelectBase>
          </Box>
        );
      }}
    </FieldArray>
  );
};
