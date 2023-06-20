import { Box, FormLabel, Stack } from "@chakra-ui/react";
import React from "react";
import { components, MultiValueGenericProps } from "react-select";
import CreatableMultiSelect from "../../../../components/select/CreatableMultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useEditViewContext } from "../../editView/EditViewContext";
import { useLinkModifiers } from "./useLinkModifiers";

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
  const { getCurrentLinkValue, setCurrentLinkValue } = useLinkModifiers();
  const fieldsValue = getCurrentLinkValue(["fields"]);
  const fields = fieldsValue
    ? (Object.keys(fieldsValue)
        .map(key => {
          if (fieldsValue[key] === undefined) {
            return null;
          }
          return {
            label: key,
            value: key
          };
        })
        .filter(Boolean) as OptionType[])
    : [];
  const addField = (field: string) => {
    setCurrentLinkValue({
      id: ["fields", field],
      value: {}
    });
  };
  const removeField = (field: string) => {
    setCurrentLinkValue({
      id: ["fields", field],
      value: undefined
    });
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
        value={fields}
        onChange={(_, action) => {
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
