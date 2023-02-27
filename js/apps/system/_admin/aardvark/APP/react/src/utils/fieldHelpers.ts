import { isNumber } from "lodash";

/**
 * This is used to escape the dot (.) in field names by putting the
 * field names that contain a dot in a string (" ... ")
 * @param field
 * @returns
 */
export const escapeFieldDot = (field?: string | number) => {
  if (field &&( field.toString().includes(".") || isNumber(field) )) {
    return `"${field.toString()}"`;
  }
  return field;
};
