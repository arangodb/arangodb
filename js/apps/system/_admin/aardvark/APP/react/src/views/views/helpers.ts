import { FormState } from "./constants";
import { useEffect, useMemo, useState } from "react";
import { DispatchArgs, State } from "../../utils/constants";
import { getPath } from "../../utils/helpers";
import { set } from "lodash";

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
