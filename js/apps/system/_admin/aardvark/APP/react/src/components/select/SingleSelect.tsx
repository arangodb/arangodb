import React from "react";

import { Props } from "react-select";
import SelectBase, { OptionType } from "./SelectBase";

const SingleSelect = (props: Props<OptionType, false>) => (
  <SelectBase {...props} />
);
export default SingleSelect;
