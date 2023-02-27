import React from "react";

import Select from "react-select/creatable";
import { Props } from "react-select";

const MultiSelect = (props: Props) => (
  <Select isMulti {...props} />
);
export default MultiSelect;
