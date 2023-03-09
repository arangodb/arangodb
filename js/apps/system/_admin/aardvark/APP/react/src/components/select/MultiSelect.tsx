import React from "react";

import { Props } from "react-select";
import SelectBase, { OptionType } from "./SelectBase";

const MultiSelect = (props: Props<OptionType>) => (
  <SelectBase {...props} isMulti />
);
export default MultiSelect;
