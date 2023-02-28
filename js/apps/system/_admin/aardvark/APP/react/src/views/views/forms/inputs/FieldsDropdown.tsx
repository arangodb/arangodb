import React from "react";
import CreatableMultiSelect from "../../../../components/pure-css/form/MultiSelect";
import { components, MultiValueGenericProps } from "react-select";
import { Link, useRouteMatch } from "react-router-dom";

const MultiValueLabel = (props: MultiValueGenericProps) => {
  const match = useRouteMatch();
  return (
    <Link
      to={`${match.url}/${props.data.value}`}
      style={{
        textDecoration: "underline",
        minWidth: 0 // because parent is flex
      }}
      title={props.data.label}
    >
      <components.MultiValueLabel {...props} />
    </Link>
  );
};

export const FieldsDropdown = ({
  fields,
  removeField,
  addField,
  disabled
}: {
  fields: { label: string; value: string }[];
  removeField: (field: string | number) => void;
  addField: (field: string | number) => void;
  disabled: boolean | undefined;
}) => {
  return (
    <CreatableMultiSelect
      value={fields}
      components={{
        MultiValueLabel
      }}
      menuPortalTarget={document.body}
      onChange={(_, action) => {
        if (action.action === "remove-value") {
          removeField((action.removedValue as any).value as string);
          return;
        }
        if (action.action === "create-option") {
          addField((action.option as any).value as string);
        }
      }}
      isDisabled={disabled}
      placeholder={"Start typing to add a field."}
    />
  );
};
