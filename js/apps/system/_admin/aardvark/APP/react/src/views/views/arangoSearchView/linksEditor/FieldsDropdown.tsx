import { Box, FormLabel, Stack } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import { get, set } from "lodash";
import React from "react";
import { components, MultiValueGenericProps } from "react-select";
import CreatableMultiSelect from "../../../../components/select/CreatableMultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useEditViewContext } from "../../editView/EditViewContext";
import { ArangoSearchViewPropertiesType } from "../../searchView.types";

const MultiValueLabelFields = (props: MultiValueGenericProps<OptionType>) => {
  const { setCurrentField, currentField } = useEditViewContext();
  return (
    <Box
      style={{
        textDecoration: "underline",
        minWidth: 0 // because parent is flex
      }}
      cursor="pointer"
      onClick={() => {
        setCurrentField([...currentField, props.data.value]);
      }}
    >
      <components.MultiValueLabel {...props} />
    </Box>
  );
};
export const FieldsDropdown = () => {
  const { currentField, currentLink = "" } = useEditViewContext();
  // convert ['test', 'test1'] to ['fields', 'test', 'fields', 'test1']
  const fieldPath = currentField.reduce((acc, curr) => {
    acc.push("fields");
    acc.push(curr);
    return acc;
  }, [] as string[]);
  const { values, setValues } =
    useFormikContext<ArangoSearchViewPropertiesType>();
  const currentLinkFieldsValue = values?.links?.[currentLink];
  const fieldsValue =
    fieldPath.length > 0
      ? get(currentLinkFieldsValue, fieldPath)?.fields
      : currentLinkFieldsValue?.fields;
  console.log({ fieldPath, currentLinkFieldsValue, fieldsValue });
  const value = fieldsValue
    ? Object.keys(fieldsValue).map(key => {
        return {
          label: key,
          value: key
        };
      })
    : [];
  const addField = (field: string) => {
    const newLinks = values.links
      ? set(values.links, [currentLink, ...fieldPath, "fields", field], {})
      : {};

    setValues({
      ...values,
      links: newLinks
    });
    // fieldsHelpers.setValue({
    //   ...fieldsField.value,
    //   [field]: {}
    // });
  };
  const removeField = (field: string) => {
    const newLinks = values.links
      ? set(
          values.links,
          [currentLink, ...fieldPath, "fields", field],
          undefined
        )
      : {};
    setValues({
      ...values,
      links: newLinks
    });
    // delete newFields[field];
    // fieldsHelpers.setValue(newFields);
  };
  return (
    <>
      <Stack direction="row" alignItems="center">
        <FormLabel margin="0" htmlFor="fields">
          Fields
        </FormLabel>
        <InfoTooltip label="Add field names that you want to be indexed here. Click on a field name to set field details." />
      </Stack>
      <CreatableMultiSelect
        inputId="fields"
        openMenuOnFocus={false}
        openMenuOnClick={false}
        placeholder="Start typing to add a field"
        noOptionsMessage={() => null}
        components={{
          MultiValueLabel: MultiValueLabelFields
        }}
        isClearable={false}
        value={value}
        onChange={(_, action) => {
          console.log("onChange", action);
          if (action.action === "remove-value") {
            removeField(action.removedValue.value);
            return;
          }
          if (
            (action.action === "select-option" ||
              action.action === "create-option") &&
            action.option?.value
          ) {
            addField(action.option.value);
          }
        }}
      />
    </>
  );
};
