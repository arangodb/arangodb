import React from "react";

import Select, { components, OptionProps, Props,  } from "react-select";
export type OptionType = {
  value: string;
  label: string;
};

const Option = (props: OptionProps<OptionType>) => {
  return (
    <div title={props.data.value}>
      <components.Option {...props} />
    </div>
  );
};

export const getSelectBase = (SelectComponent: Select) => (props: Props<OptionType>) => (
  <SelectComponent
    {...props}
    menuPortalTarget={document.body}
    components={{
      ...props.components,
      Option
    }}
    styles={{
      ...props.styles,
      option: baseStyles => ({
        ...baseStyles,
        overflow: "hidden",
        textOverflow: "ellipsis"
      }),
      input: baseStyles => ({
        ...baseStyles,
        "> input": {
          background: "transparent !important"
        }
      })
    }}
  />
);

const SelectBase = getSelectBase(Select);
export default SelectBase;
