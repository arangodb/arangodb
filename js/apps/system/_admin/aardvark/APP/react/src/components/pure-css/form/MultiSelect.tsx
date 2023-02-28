import React from "react";

import Select, { components, OptionProps, Props } from "react-select";

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
const MultiSelect = (props: Props<OptionType>) => (
  <Select
    isMulti
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
    {...props}
  />
);
export default MultiSelect;
