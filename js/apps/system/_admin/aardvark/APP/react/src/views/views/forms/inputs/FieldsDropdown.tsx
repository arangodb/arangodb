import { Box } from "@chakra-ui/react";
import React, { useContext } from "react";
import { components, MultiValueGenericProps } from "react-select";
import CreatableMultiSelect from "../../../../components/select/CreatableMultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { escapeFieldDot } from "../../../../utils/fieldHelpers";
import { LinkProperties, ViewContext } from "../../constants";
import { useLinksContext } from "../../LinksContext";

const MultiValueLabel = (props: MultiValueGenericProps<OptionType>) => {
  const { setCurrentField, currentField } = useLinksContext();
  return (
    <Box
      style={{
        textDecoration: "underline",
        minWidth: 0 // because parent is flex
      }}
      cursor="pointer"
      title={props.data.label}
      onClick={() => {
        if (!currentField) {
          return;
        }
        setCurrentField({
          fieldPath: `${currentField.fieldPath}.fields[${props.data.value}]`
        });
      }}
    >
      <components.MultiValueLabel {...props} />
    </Box>
  );
};

export const FieldsDropdown = ({
  isDisabled,
  basePath,
  formState
}: {
  basePath: string;
  isDisabled: boolean;
  formState: LinkProperties;
}) => {
  const viewContext = useContext(ViewContext);
  const { dispatch } = viewContext;
  const addField = (field: string | number) => {
    const newField = escapeFieldDot(field);
    dispatch({
      type: "setField",
      field: {
        path: `fields[${newField}]`,
        value: {}
      },
      basePath
    });
  };

  const removeField = (field: string | number) => {
    const newField = escapeFieldDot(field);
    dispatch({
      type: "unsetField",
      field: {
        path: `fields[${newField}]`
      },
      basePath
    });
  };

  const fieldKeys = Object.keys(formState.fields || {});
  const fields = fieldKeys.map(key => {
    return {
      label: key,
      value: key
    };
  });

  return (
    <CreatableMultiSelect
      value={fields}
      isClearable={false}
      noOptionsMessage={() => null}
      components={{
        MultiValueLabel
      }}
      onChange={(_, action) => {
        if (action.action === "remove-value") {
          removeField(action.removedValue.value);
          return;
        }
        if (action.action === "create-option") {
          addField(action.option.value);
        }
      }}
      isDisabled={isDisabled}
      placeholder={"Start typing to add a field."}
    />
  );
};
