import { Box, FormLabel } from "@chakra-ui/react";
import { useField } from "formik";
import React from "react";
import { components, MultiValueGenericProps, MultiValue } from "react-select";
import { CreatableMultiSelectControl } from "../../../../components/form/CreatableMultiSelectControl";
import { OptionType } from "../../../../components/select/SelectBase";
import { IndexFormField, IndexFormFieldProps } from "./IndexFormField";
import { InfoTooltip } from "./InfoTooltip";
import { InvertedIndexAnalyzerDropdown } from "./InvertedIndexAnalyzerDropdown";
import { FieldData, useInvertedIndexContext } from "./InvertedIndexContext";
import {
  invertedIndexFieldsMap,
  InvertedIndexFieldType
} from "./useCreateInvertedIndex";

const MultiValueLabel = (
  props: MultiValueGenericProps<OptionType> & { fieldPath: string }
) => {
  const { setCurrentFieldData } = useInvertedIndexContext();
  const { fieldPath, ...rest } = props;
  return (
    <Box
      onClick={() => {
        const fieldData = { name: props.data.value, path: fieldPath };
        console.log({ name: fieldData.name, path: fieldData.path });
        setCurrentFieldData(fieldData);
      }}
      textDecoration="underline"
      cursor="pointer"
    >
      <components.MultiValueLabel {...rest} />
    </Box>
  );
};

const InvertedIndexFieldsDropdown = ({
  field,
  autoFocus,
  children
}: {
  autoFocus: boolean;
  field: IndexFormFieldProps;
  children?: React.ReactNode;
}) => {
  const { currentFieldData } = useInvertedIndexContext();
  const [formikField] = useField<InvertedIndexFieldType[]>(field.name);
  const index = formikField.value?.findIndex(
    fieldData => fieldData.name === currentFieldData?.name
  );
  let newIndex = index && index !== -1 ? index : 0;
  const fullPath = `${field.name}.[${newIndex}]`;
  console.log({ fullPath, value: formikField.value });
  return (
    <Box
      gridColumn="1 / span 3"
      background="grey.100"
      border="2px solid"
      borderRadius="md"
      borderColor="gray.200"
      padding="2"
      maxWidth="800px"
    >
      <FieldsDropdown fullPath={fullPath} field={field} autoFocus={autoFocus} />
      <Box>Field: {currentFieldData?.path} </Box>
      {children}
    </Box>
  );
};

export const InvertedIndexFieldsDropdownWrap = ({
  field,
  autoFocus
}: {
  autoFocus: boolean;
  field: IndexFormFieldProps;
}) => {
  const { currentFieldData } = useInvertedIndexContext();
  const [formikField] = useField<InvertedIndexFieldType[]>(field.name);
  const index = formikField.value?.findIndex(
    fieldData => fieldData.name === currentFieldData?.name
  );
  let newIndex = index && index !== -1 ? index : 0;
  const fullPath = `${field.name}.[${newIndex}]`;
  const isTopLevel = currentFieldData?.path.startsWith(`fields`);
  const showFieldDetailsForm = isTopLevel;

  console.log({
    path: currentFieldData?.path,
    fullPath,
    isTopLevel
  });
  return (
    <InvertedIndexFieldsDropdown field={field} autoFocus={autoFocus}>
      {currentFieldData && showFieldDetailsForm ? (
        <FieldDataForm
          currentFieldData={currentFieldData}
          index={newIndex}
          fullPath={fullPath}
        />
      ) : null}
    </InvertedIndexFieldsDropdown>
  );
};

/**
 * 
{
  name: <name:string>,
  analyzer: <analyzerName:string, optional>
  features: <analyzerFeatures: array, possible values [ "frequency", "position", "offset", "norm"], optional, default []>   
  searchField: <bool, optional>
  includeAllFields: <bool, optional>
  trackListPositions: <bool, optional>          
  nested: [
    // see below, EE only, optional
    ],
}

 */

const FieldDataForm = ({
  currentFieldData,
  fullPath
}: {
  currentFieldData: FieldData;
  index: number;
  fullPath: string;
}) => {
  return (
    <Box padding="4">
      <Box fontSize="lg" marginBottom="2">
        Field: {currentFieldData.name}
      </Box>
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
        <IndexFormField
          render={({ field }) => {
            return (
              <InvertedIndexFieldsDropdown autoFocus={true} field={field} />
            );
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
  );
};
const FieldsDropdown = ({
  field,
  fullPath,
  autoFocus
}: {
  field: IndexFormFieldProps;
  fullPath: string;
  autoFocus: boolean;
}) => {
  const [formikField, , fieldHelpers] = useField<InvertedIndexFieldType[]>(
    field.name
  );
  console.log({ name: field.name, fullPath });
  return (
    <Box
      display="grid"
      gridTemplateColumns="200px 1fr 40px"
      columnGap="3"
      alignItems="center"
    >
      <FormLabel htmlFor={field.name}>{field.label}</FormLabel>
      <CreatableMultiSelectControl
        isDisabled={field.isDisabled}
        selectProps={{
          autoFocus,
          value: formikField.value?.map(indexField => {
            return { label: indexField.name, value: indexField.name };
          }),
          components: {
            MultiValueLabel: props => (
              <MultiValueLabel fieldPath={fullPath} {...props} />
            )
          },
          onChange: values => {
            // set values to {name: ''} object
            const updatedValue = (values as MultiValue<OptionType>)?.map(
              value => {
                const existingValues = formikField.value?.find(fieldValue => {
                  return fieldValue.name === value.value;
                });
                return { name: value.value, ...existingValues };
              }
            );
            fieldHelpers.setValue(updatedValue);
          },
          noOptionsMessage: () => "Start typing to add a field"
        }}
        isRequired={field.isRequired}
        name={field.name}
      />
      {field.tooltip ? <InfoTooltip label={field.tooltip} /> : <Box></Box>}
    </Box>
  );
};
