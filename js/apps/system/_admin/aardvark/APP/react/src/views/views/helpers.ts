import Ajv2019 from "ajv/dist/2019";
import ajvErrors from 'ajv-errors';
import { formSchema, linksSchema } from "./constants";
import { useEffect, useMemo, useState } from "react";

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
