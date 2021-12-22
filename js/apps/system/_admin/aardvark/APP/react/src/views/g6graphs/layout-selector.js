import React from "react";
import { Select, Icon } from "antd";

const SelectOption = Select.Option;
const LayoutSelector = props => {
  const { apis, value, onChange } = props;
  const { layouts } = apis.getInfo();
  return (
    <div style={{ position: "absolute", top: 10, left: 10 }}>
      <Select style={{ width: "90%" }} value={value} onChange={onChange}>
        {layouts.map(item => {
          const { name, icon, disabled, desc } = item;
          return (
            <SelectOption key={name} value={name} disabled={disabled}>
              <Icon type={icon} /> &nbsp;{desc}
            </SelectOption>
          );
        })}
      </Select>
    </div>
  );
};
export default LayoutSelector;
