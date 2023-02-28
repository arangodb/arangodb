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
  <Select isMulti components={{
    ...props.components,
    Option
  }} {...props} />
);
export default MultiSelect;
