import Ajv2019 from 'ajv/dist/2019';
import ajvErrors from 'ajv-errors';
import { formSchema, FormState, linksSchema } from './constants';
import { useEffect, useMemo, useState } from 'react';
import { DispatchArgs, State } from '../../utils/constants';
import { getPath } from '../../utils/helpers';
import { chain, cloneDeep, escape, get, isNull, merge, omit, set, truncate } from 'lodash';
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
  const { data, error } = useSWR(`/view/${name}/properties`,
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

export const postProcessor = (state: State<FormState>, action: DispatchArgs<FormState>, setChanged: (changed: boolean) => void, oldName: string) => {
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

  if (['setFormState', 'setField', 'unsetField'].includes(action.type)) {
    window.sessionStorage.setItem(oldName, JSON.stringify(state.formState));
    window.sessionStorage.setItem(`${oldName}-changed`, "true");
    setChanged(true);
  }
};

export const buildSubNav = (isAdminUser: boolean, name: string, activeKey: string, changed: boolean) => {
  let breadCrumb = 'View: ' + escape(truncate(name, { length: 64 }));
  if (!isAdminUser) {
    breadCrumb += ' (read-only)';
  } else if (changed) {
    breadCrumb += '* (unsaved changes)';
  }

  const defaultRoute = '#view/' + encodeURIComponent(name);
  const menus: { [key: string]: any } = {

    Settings: {
      route: defaultRoute
    },
    Links: {
      route: `${defaultRoute}/links`
    },
    // 'Consolidation Policy': {
    //   route: `${defaultRoute}/consolidation`
    // },
    // Info: {
    //   route: `${defaultRoute}/info`
    // },
    JSON: {
      route: `${defaultRoute}/json`
    }
  };

  menus[activeKey].active = true;

  const $ = window.$;

  // Directly render subnav when container divs already exist.
  // This is used during client-side navigation.
  $('#subNavigationBar .breadcrumb').html(breadCrumb);
  arangoHelper.buildSubNavBar(menus);

  // Setup observer to watch for container divs creation, then render subnav.
  // This is used during direct page loads or a page refresh.
  const target = $("#subNavigationBar")[0];
  const observer = new MutationObserver(function (mutations) {
    mutations.forEach(function (mutation) {
      const newNodes = mutation.addedNodes; // DOM NodeList
      if (newNodes !== null) { // If there are new nodes added
        const $nodes = $(newNodes); // jQuery set
        $nodes.each(function (_idx: number, node: Element) {
          const $node = $(node);
          if ($node.hasClass("breadcrumb")) {
            $node.html(breadCrumb);
          } else if ($node.hasClass("bottom")) {
            arangoHelper.buildSubNavBar(menus);
          }
        });
      }
    });
  });

  const config = {
    attributes: true,
    childList: true,
    characterData: true
  };

  observer.observe(target, config);

  return observer;
};

export function useNavbar (name: string, isAdminUser: boolean, changed: boolean, key: string) {
  useEffect(() => {
    const observer = buildSubNav(isAdminUser, name, key, changed);

    return () => observer.disconnect();
  });
}
