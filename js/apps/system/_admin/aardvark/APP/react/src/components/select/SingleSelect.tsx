import React from "react";

import Select, { SelectInstance, Props } from "react-select";
import { getSelectBase, OptionType } from "./SelectBase";

const SingleSelectBase = getSelectBase<false>(Select);

const SingleSelect = React.forwardRef(
  (
    props: Props<OptionType, false>,
    ref: React.Ref<SelectInstance<OptionType, false>>
  ) => <SingleSelectBase {...props} ref={ref} />
);
export default SingleSelect;
