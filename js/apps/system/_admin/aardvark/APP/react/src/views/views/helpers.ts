import Ajv2019 from 'ajv/dist/2019';
import ajvErrors from 'ajv-errors';
import { formSchema, FormState, linksSchema } from './constants';
import { useEffect, useMemo, useState } from 'react';
import { DispatchArgs, State } from '../../utils/constants';
import { getPath } from '../../utils/helpers';
import { chain, cloneDeep, escape, isNull, merge, omit, set, truncate } from 'lodash';
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../utils/arangoClient";

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
  const [field, setField] = useState('');
  const [addDisabled, setAddDisabled] = useState(true);
  const fields = useMemo(() => (formState[formField] || {}), [formField, formState]);

  useEffect(() => {
    const fieldKeys = chain(fields).omitBy(isNull).keys().value();

    setAddDisabled(!field || fieldKeys.includes(field));
  }, [field, fields]);

  return [field, setField, addDisabled, fields];
}

export function useView (name: string) {
  const [view, setView] = useState<object>({ name });
  const { data } = useSWR(`/view/${name}/properties`,
    path => getApiRouteForCurrentDB().get(path), {
      revalidateOnFocus: false
    });

  useEffect(() => {
    if (data) {
      setView(omit(data.body, 'error', 'code'));
    }
  }, [data]);

  return view;
}

export const postProcessor = (state: State<FormState>, action: DispatchArgs<FormState>) => {
  if (action.type === 'setField' && action.field) {
    const path = getPath(action.basePath, action.field.path);

    if (action.field.path === 'consolidationPolicy.type') {
      const tempFormState = cloneDeep(state.formCache);

      validateAndFix(tempFormState);
      state.formState = tempFormState as unknown as FormState;

      merge(state.formCache, state.formState);
    } else if (action.field.value !== undefined) {
      set(state.formState, path, action.field.value);
    }
  }
};

export const buildSubNav = (isAdminUser: boolean, name: string, activeKey: string) => {
  let breadCrumb = 'View: ' + escape(truncate(name, { length: 64 }));
  if (!isAdminUser) {
    breadCrumb += ' (read-only)';
  }

  const defaultRoute = '#view/' + encodeURIComponent(name);
  const menus: { [key: string]: any } = {
    Info: {
      route: defaultRoute
    },
    Settings: {
      route: `${defaultRoute}/settings`
    },
    Consolidation: {
      route: `${defaultRoute}/consolidation`
    },
    Links: {
      route: `${defaultRoute}/links`
    },
    JSON: {
      route: `${defaultRoute}/json`
    }
  };

  menus[activeKey].active = true;

  window.setImmediate(() => {
    window.$('#subNavigationBar .breadcrumb').html(breadCrumb);
    arangoHelper.buildSubNavBar(menus);
  });
};
