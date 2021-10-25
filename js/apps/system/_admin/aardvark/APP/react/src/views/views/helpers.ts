import Ajv2019 from "ajv/dist/2019";
import ajvErrors from 'ajv-errors';
import { formSchema, FormState, linksSchema } from "./constants";
import { useEffect, useMemo, useState } from "react";
import { DispatchArgs, State } from "../../utils/constants";
import { getPath } from "../../utils/helpers";
import { set } from "lodash";

const ajv = new Ajv2019({
  allErrors: true,
  removeAdditional: 'failing',
  useDefaults: true,
  discriminator: true,
  $data: true
});
ajvErrors(ajv);

export const validateAndFix = ajv.addSchema(linksSchema).compile(formSchema);

export function useLinkState (formState: { [key: string]: any }, formField: string) {
  const [field, setField] = useState('');
  const [addDisabled, setAddDisabled] = useState(true);
  const fields = useMemo(() => (formState[formField] || {}), [formField, formState]);

  useEffect(() => {
    setAddDisabled(!field || Object.keys(fields).includes(field));
  }, [field, fields]);

  return [field, setField, addDisabled, fields];
}

export const postProcessor = (state: State<FormState>, action: DispatchArgs<FormState>) => {
  if (action.type === 'setField' && action.field && action.field.value !== undefined) {
    const path = getPath(action.basePath, action.field.path);

    set(state.formState, path, action.field.value);
  }
};
