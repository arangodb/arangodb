import React from "react";
import { GroupBase } from "react-select";
import AsyncSelect, { AsyncProps } from "react-select/async";
import { getSelectBase, OptionType } from "./SelectBase";

const AsyncMultiSelectBase = getSelectBase<true>(AsyncSelect);

const AsyncMultiSelect = (
  props: AsyncProps<OptionType, true, GroupBase<OptionType>>
) => <AsyncMultiSelectBase {...props} isMulti />;
export default AsyncMultiSelect;
