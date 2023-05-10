import Ajv2019 from 'ajv/dist/2019';
import ajvErrors from 'ajv-errors';
import { formSchema, FormState, linksSchema } from './constants';
import { useEffect, useMemo, useState } from 'react';
import { DispatchArgs, State } from '../../utils/constants';
import { getPath } from '../../utils/helpers';
import { chain, cloneDeep, get, isNull, merge, omit, set, uniqueId } from 'lodash';
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";
import { fixFieldsInit } from './reducerHelper';
import { encodeHelper } from '../../utils/encodeHelper';

declare var arangoHelper: { [key: string]: any };
declare var window: any;

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
  const [innerField, setInnerField] = useState('');
  const [addDisabled, setAddDisabled] = useState(true);
  const innerFields = useMemo(() => get(formState, formField, {}), [formField, formState]);

  useEffect(() => {
    const innerFieldKeys = chain(innerFields).omitBy(isNull).keys().value();

    setAddDisabled(!innerField || innerFieldKeys.includes(innerField));
  }, [innerField, innerFields]);

  return [innerField, setInnerField, addDisabled, innerFields];
}

export function useView (name: string) {
  const [view, setView] = useState<Partial<FormState>>({ name });
  const { encoded: encodedName } = encodeHelper(name);
  const { data, error } = useSWR(`/view/${encodedName}/properties`,
    path => getApiRouteForCurrentDB().get(path), {
      revalidateOnFocus: false
    });

  if (error) {
    window.App.navigate('#views', { trigger: true });
    arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${error.errorNum}: ${error.message} - ${name}`
    );
  }

  useEffect(() => {
    if (data) {
      let viewState = omit(data.body, 'error', 'code') as FormState;

      const cachedViewStateStr = window.sessionStorage.getItem(name);
      if (cachedViewStateStr) {
        viewState = JSON.parse(cachedViewStateStr);
      }

      setView(viewState);
    }
  }, [data, name]);

  return view;
}

/**
 * called after the reducer, 
 * modifies "formState" (while reducer deals with "formCache")
 */
export const postProcessor = (state: State<FormState>, action: DispatchArgs<FormState>, setChanged: (changed: boolean) => void, oldName: string) => {
  if (action.type === 'setField' && action.field) {
    const path = getPath(action.basePath, action.field.path);

    if (action.field.path === 'consolidationPolicy.type') {
      const tempFormState = cloneDeep(state.formCache);

      validateAndFix(tempFormState);
      state.formState = tempFormState as unknown as FormState;

      merge(state.formCache, state.formState);
    } else if (action.field.value !== undefined) {
      fixFieldsInit(state.formState, action);
      set(state.formState, path, action.field.value);
    }
  }

  if (['setFormState', 'setField', 'unsetField'].includes(action.type)) {
    window.sessionStorage.setItem(oldName, JSON.stringify(state.formState));
    window.sessionStorage.setItem(`${oldName}-changed`, "true");
    setChanged(true);
  }

  if (['setField', 'unsetField'].includes(action.type)) {
    state.renderKey = uniqueId('force_re-render_');
  }
};

