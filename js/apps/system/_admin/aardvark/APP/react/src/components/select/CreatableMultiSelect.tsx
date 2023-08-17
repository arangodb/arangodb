import React from "react";

import { Props } from "react-select";
import CreatableSelect from "react-select/creatable";
import { getSelectBase, OptionType } from "./SelectBase";

const CreatableMultiSelectBase = getSelectBase<true>(CreatableSelect);

const CreatableMultiSelect = (props: Props<OptionType, true>) => (
  <CreatableMultiSelectBase {...props} isMulti />
);
export default CreatableMultiSelect;
