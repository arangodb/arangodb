import React from "react";

import { Props } from "react-select";
import { CreatableSelectBase, OptionType } from "./SelectBase";

const CreatableMultiSelect = (props: Props<OptionType>) => (
  <CreatableSelectBase {...props} isMulti />
);
export default CreatableMultiSelect;
