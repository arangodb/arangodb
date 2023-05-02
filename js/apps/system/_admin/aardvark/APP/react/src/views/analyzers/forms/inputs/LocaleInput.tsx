import React from "react";
import { InputControl } from "../../../../components/form/InputControl";

export const LocaleInput = () => {
  return (
    <InputControl
      name="properties.locale"
      inputProps={{
        placeholder: "language[_COUNTRY][.encoding][@variant]"
      }}
      label="Locale"
    />
  );
};
