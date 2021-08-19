import Ajv from "ajv";
import { formSchema } from "./constants";

const ajv = new Ajv({
  removeAdditional: 'failing',
  useDefaults: true,
  discriminator: true,
  $data: true
});
export const validateAndFix = ajv.compile(formSchema);

export const getPath = (basePath: string | undefined, path: string) => basePath ? `${basePath}.${path}` : path;
