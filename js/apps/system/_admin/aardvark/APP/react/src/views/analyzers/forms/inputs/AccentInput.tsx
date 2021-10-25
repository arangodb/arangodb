import React from "react";
import { FormProps } from "../../../../utils/constants";
import { AccentProperty } from "../../constants";
import Checkbox from "../../../../components/pure-css/form/Checkbox";
import { getBooleanFieldSetter } from "../../../../utils/helpers";

type AccentInputProps = FormProps<AccentProperty> & {
  inline?: boolean;
};

const AccentInput = ({ formState, dispatch, disabled, inline = true }: AccentInputProps) =>
  <Checkbox onChange={getBooleanFieldSetter('properties.accent', dispatch)} label={'Accent'} inline={inline}
            disabled={disabled} checked={formState.properties.accent}/>;

export default AccentInput;
