import React from "react";

import { Props } from "react-select";
import CreatableSelect from "react-select/creatable";
import { getSelectBase, OptionType } from "./SelectBase";

const CreatableSelectBase = getSelectBase(CreatableSelect);
const CreatableMultiSelect = (props: Props<OptionType>) => (
  <CreatableSelectBase {...props} isMulti />
);
export default CreatableMultiSelect;
