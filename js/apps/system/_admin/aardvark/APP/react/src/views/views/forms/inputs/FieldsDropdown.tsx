import React, { useContext } from "react";
import { Link, useRouteMatch } from "react-router-dom";
import { components, MultiValueGenericProps } from "react-select";
import CreatableMultiSelect from "../../../../components/select/CreatableMultiSelect";
import { OptionType } from "../../../../components/select/SelectBase";
import { escapeFieldDot } from "../../../../utils/fieldHelpers";
import { LinkProperties, ViewContext } from "../../constants";

const MultiValueLabel = (props: MultiValueGenericProps<OptionType>) => {
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
          removeField((action.removedValue as any).value as string);
          return;
        }
        if (action.action === "create-option") {
          addField((action.option as any).value as string);
        }
      }}
      isDisabled={isDisabled}
      placeholder={"Start typing to add a field."}
    />
  );
};
