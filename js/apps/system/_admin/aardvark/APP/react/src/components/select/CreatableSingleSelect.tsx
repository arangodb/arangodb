import React from "react";

import { Props } from "react-select";
import CreatableReactSelect from "react-select/creatable";
import { getSelectBase, OptionType } from "./SelectBase";

const CreatableSelectBase = getSelectBase<false>(CreatableReactSelect);

const CreatableSingleSelect = (props: Props<OptionType, false>) => (
  <CreatableSelectBase {...props} />
);
export default CreatableSingleSelect;
