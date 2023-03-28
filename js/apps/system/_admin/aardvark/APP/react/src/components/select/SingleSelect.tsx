import React from "react";

import { Props } from "react-select";
import SelectBase, { OptionType } from "./SelectBase";

const SingleSelect = (props: Props<OptionType, false>) => (
  <SelectBase {...props} isMulti={false} />
);
export default SingleSelect;
