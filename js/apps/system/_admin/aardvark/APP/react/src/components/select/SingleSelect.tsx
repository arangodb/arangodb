import React from "react";

import Select, { Props } from "react-select";
import { getSelectBase, OptionType } from "./SelectBase";

const SingleSelectBase = getSelectBase<false>(Select);

const SingleSelect = (props: Props<OptionType, false>) => (
  <SingleSelectBase {...props} />
);
export default SingleSelect;
