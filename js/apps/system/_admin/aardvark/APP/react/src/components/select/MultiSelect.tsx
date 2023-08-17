import React from "react";

import Select, { Props } from "react-select";
import { getSelectBase, OptionType } from "./SelectBase";

const MultiSelectBase = getSelectBase<true>(Select);
const MultiSelect = (props: Props<OptionType, true>) => (
  <MultiSelectBase {...props} isMulti />
);
export default MultiSelect;
