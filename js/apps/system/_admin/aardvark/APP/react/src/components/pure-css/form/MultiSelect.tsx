import React from "react";

import Select, { Props } from "react-select";

const MultiSelect = <T extends unknown>(props: Props<T>) => (
  <Select isMulti {...props} />
);
export default MultiSelect;
