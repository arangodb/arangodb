import { get, set } from "lodash";
import { DispatchArgs } from "../../utils/constants";

/**
 * Initilizes 'fields' object if it doesn't exist
 * Fixes the issue where entering a number in 'fields' 
 * creates an array with `null` values
 * Required in two places - postProcessor + reducer
 */
export const fixFieldsInit = (
  formStateOrFormCache: object,
  action: DispatchArgs<object>
) => {
  const fieldsPath =
    action.basePath &&
    action.basePath.startsWith("links") &&
    `${action.basePath}.fields`;
  if (fieldsPath && !get(formStateOrFormCache, fieldsPath)) {
    set(formStateOrFormCache, fieldsPath, {});
  }
};
