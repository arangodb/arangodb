import React from "react";

import CreatableSelect from "react-select/creatable";
import { Props } from "react-select";

const CreatableMultiSelect = (props: Props) => (
  <CreatableSelect isMulti {...props} />
);
export default CreatableMultiSelect;
