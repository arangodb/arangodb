import { ChangeEvent, Dispatch, useCallback, useEffect, useRef } from "react";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "./arangoClient";
import minimatch from "minimatch";
import { cloneDeep, compact, has, isEqual, merge, set, uniqueId, unset } from "lodash";
import { DispatchArgs, State } from "./constants";
import { ErrorObject, ValidateFunction } from "ajv";

declare var frontendConfig: { [key: string]: any };
declare var arangoHelper: { [key: string]: any };

export const getChangeHandler = (setter: (value: string) => void) => {
  return (event: ChangeEvent<HTMLInputElement | HTMLSelectElement>) => {
    setter(event.currentTarget.value);
  };
};

export const usePrevious = (value: any) => {
  const ref = useRef();

  useEffect(() => {
    ref.current = value;
  });

  return ref.current;
};


export const usePermissions = () => useSWR(
  `/user/${arangoHelper.getCurrentJwtUsername()}/database/${frontendConfig.db}`,
  (path) => getApiRouteForCurrentDB().get(path)
);

export const facetedFilter = (filterExpr: string, list: { [key: string]: any }[], facets: string[]) => {
  let filteredList = list;

  if (filterExpr) {
    try {
      const filters = filterExpr.trim().split(/\s+/);

      for (const filter of filters) {
        const splitIndex = filter.indexOf(':');
        const field = filter.slice(0, splitIndex);
        const pattern = filter.slice(splitIndex + 1);

        filteredList = filteredList.filter(
          item => minimatch(item[field].toLowerCase(), `*${pattern.toLowerCase()}*`));
      }
    } catch (e) {
      const normalizedPattern = `*${filterExpr.toLowerCase()}*`;

      filteredList = filteredList.filter(item => facets.some(field => minimatch(item[field].toLowerCase(), normalizedPattern)));
    }
  }

  return filteredList;
};

export const getPath = (basePath: string | undefined, path: string | undefined) =>
  compact([basePath, path]).join('.');

export const getReducer = <FormState extends object> (initialState: State<FormState>,
                                                      postProcessor?: (state: State<FormState>, action: DispatchArgs<FormState>) => void) =>
  (state: State<FormState>, action: DispatchArgs<FormState>): State<FormState> => {
    let newState = state;

    switch (action.type) {
      case 'lockJsonForm':
        newState = {
          ...state,
          lockJsonForm: true
        };
        break;

      case 'unlockJsonForm':
        newState = {
          ...state,
          lockJsonForm: false
        };
        break;

      case 'show':
        newState = {
          ...state,
          show: true
        };
        break;

      case 'showJsonForm':
        newState = {
          ...state,
          showJsonForm: true
        };
        break;

      case 'hideJsonForm':
        newState = {
          ...state,
          showJsonForm: false
        };
        break;

      case 'regenRenderKey':
        newState = {
          ...state,
          renderKey: uniqueId('force_re-render_')
        };
        break;

      case 'setField':
        if (action.field && action.field.value !== undefined) {
          newState = cloneDeep(state);
          const path = getPath(action.basePath, action.field.path);

          set(newState.formCache, path, action.field.value);
        }
        break;

      case 'unsetField':
        if (action.field) {
          newState = cloneDeep(state);
          const path = getPath(action.basePath, action.field.path);

          unset(newState.formState, path);
          unset(newState.formCache, path);
        }
        break;

      case 'setFormState':
        if (action.formState) {
          newState = cloneDeep(state);
          newState.formState = action.formState;
          merge(newState.formCache, newState.formState);
        }
        break;

      case 'reset':
        newState = initialState;
        break;
    }

    if (postProcessor) {
      postProcessor(newState, action);
    }

    return newState;
  };

export function getNumericFieldValue (value: number | undefined): number | string {
  return value === undefined ? '' : value;
}

export function getNumericFieldSetter<FormState> (field: string, dispatch: Dispatch<DispatchArgs<FormState>>,
                                                  basePath?: string) {
  return (event: ChangeEvent<HTMLInputElement>) => {
    const numValue = parseFloat(event.target.value);

    if (Number.isNaN(numValue)) {
      dispatch({
        type: 'unsetField',
        field: {
          path: field
        },
        basePath
      });
    } else {
      dispatch({
        type: 'setField',
        field: {
          path: field,
          value: numValue
        },
        basePath
      });
    }
  };
}

export function getBooleanFieldSetter<FormState> (field: string, dispatch: Dispatch<DispatchArgs<FormState>>,
                                                  basePath?: string) {
  return (event: ChangeEvent<HTMLInputElement>) => {
    dispatch({
      type: 'setField',
      field: {
        path: field,
        value: event.target.checked
      },
      basePath
    });
  };
}

export function useJsonFormErrorHandler<FormState> (dispatch: Dispatch<DispatchArgs<FormState>>,
                                                    setFormErrors: (value: string[]) => void) {
  return useCallback((errors: ErrorObject[] | null | undefined) => {
    if (Array.isArray(errors)) {
      dispatch({ type: 'lockJsonForm' });

      setFormErrors(errors.map(error => {
          if (has(error.params, 'errors')) {
            return `
              ${error.params.errors[0].keyword} error: ${error.instancePath}${error.message}.
            `;
          } else {
            return `
              ${error.keyword} error: ${error.instancePath} ${error.message}.
              Schema: ${JSON.stringify(error.params)}
            `;
          }
        }
      ));
    }
  }, [dispatch, setFormErrors]);
}

export function useJsonFormUpdateEffect<FormState> (validate: ValidateFunction<FormState>, formState: FormState,
                                                    raiseError: (errors: ErrorObject[] | null | undefined) => void) {
  const prevFormState = usePrevious(formState);

  useEffect(() => {
    if (!isEqual(prevFormState, formState) && !validate(formState)) {
      raiseError(validate.errors);
    }
  }, [formState, prevFormState, raiseError, validate]);
}

