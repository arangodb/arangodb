import { Box, FormLabel, Spacer } from "@chakra-ui/react";
import { FieldArray, useField } from "formik";
import { isArray } from "lodash";
import React from "react";
import { components, MultiValueGenericProps } from "react-select";
import { FormFieldProps } from "../../../../../components/form/FormField";
import CreatableMultiSelect from "../../../../../components/select/CreatableMultiSelect";
import { OptionType } from "../../../../../components/select/SelectBase";
import { IndexInfoTooltip } from "../IndexInfoTooltip";
import { useInvertedIndexContext } from "./InvertedIndexContext";
import { InvertedIndexValuesType } from "./useCreateInvertedIndex";

const MultiValueLabel = (
  props: MultiValueGenericProps<OptionType> & {
    fieldName: string;
    fieldIndex: number;
    isBasicField?: boolean;
  }
) => {
  const { setCurrentFieldData, currentFieldData } = useInvertedIndexContext();
  const { fieldName, fieldIndex, isBasicField, ...rest } = props;
  const isSelected =
    currentFieldData?.fieldIndex === fieldIndex &&
    currentFieldData.fieldName === fieldName;
  return (
    <Box
      onClick={() => {
        if (isBasicField) {
          return;
        }
        const fieldData = {
          fieldValue: props.data.value,
          fieldName,
          fieldIndex
        };
        setCurrentFieldData(fieldData);
      }}
      textDecoration={isBasicField ? "" : "underline"}
      cursor={isBasicField ? "" : "pointer"}
      backgroundColor={isSelected ? "blue.100" : undefined}
    >
      <components.MultiValueLabel {...rest} />
    </Box>
  );
};

const getDropdownValue = (fieldValue: InvertedIndexValuesType[]) => {
  if (fieldValue && isArray(fieldValue)) {
    return fieldValue.map(data => {
      if (typeof data === "string") {
        return { label: data, value: data };
      }
      return { label: data.name || "", value: data.name || "" };
    });
  }
  return [];
};

export const FieldsDropdown = ({ field }: { field: FormFieldProps }) => {
  const [formikField] = useField<InvertedIndexValuesType[]>(field.name);
  const { setCurrentFieldData } = useInvertedIndexContext();
  const dropdownValue = getDropdownValue(formikField.value);

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
            <CreatableMultiSelect
              isDisabled={field.isDisabled}
              name={field.name}
              inputId={field.name}
              openMenuOnFocus
              components={{
                MultiValueLabel: props => {
                  const index = formikField.value?.findIndex(
                    fieldObj => fieldObj.name === props.data.value
                  );
                  // if it follows the basic field type, we don't want to allow clicking
                  const isBasicField = formikField.value.some(
                    value => typeof value === "string"
                  );
                  return (
                    <MultiValueLabel
                      isBasicField={isBasicField}
                      fieldName={field.name}
                      fieldIndex={index}
                      {...props}
                    />
                  );
                }
              }}
              isClearable={false}
              noOptionsMessage={() => "Start typing to add a field"}
              onChange={(_, action) => {
                if (action.action === "create-option") {
                  push({ name: action.option.value });
                }
                if (
                  action.action === "remove-value" ||
                  action.action === "pop-value"
                ) {
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
            />
            {field.tooltip ? (
              <IndexInfoTooltip label={field.tooltip} />
            ) : (
              <Spacer />
            )}
          </Box>
        );
      }}
    </FieldArray>
  );
};
