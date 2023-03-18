import { Box, FormLabel } from "@chakra-ui/react";
import { FieldArray, useField } from "formik";
import React from "react";
import { components, MultiValueGenericProps } from "react-select";
import {
  CreatableSelectBase,
  OptionType
} from "../../../../../components/select/SelectBase";
import { IndexFormFieldProps } from "../IndexFormField";
import { useInvertedIndexContext } from "./InvertedIndexContext";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

const MultiValueLabel = (
  props: MultiValueGenericProps<OptionType> & {
    fieldName: string;
    fieldIndex: number;
  }
) => {
  const { setCurrentFieldData, currentFieldData } = useInvertedIndexContext();
  const { fieldName, fieldIndex, ...rest } = props;
  const isSelected =
    currentFieldData?.fieldIndex === fieldIndex &&
    currentFieldData.fieldName === fieldName;
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
      backgroundColor={isSelected ? "blue.100" : undefined}
    >
      <components.MultiValueLabel {...rest} />
    </Box>
  );
};
export const FieldsDropdown = ({ field }: { field: IndexFormFieldProps }) => {
  const [formikField] = useField<InvertedIndexValuesType[]>(field.name);
  const { setCurrentFieldData } = useInvertedIndexContext();
  const dropdownValue =
    formikField.value?.map(data => {
      return { label: data.name || "", value: data.name || "" };
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
              name={field.name}
              inputId={field.name}
              openMenuOnFocus
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
                    // clears the state when removing item from top level fields
                    if (field.name === "fields") {
                      setCurrentFieldData(undefined);
                    }
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
