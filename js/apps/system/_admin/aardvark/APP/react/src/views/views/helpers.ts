import Ajv2019 from "ajv/dist/2019";
import ajvErrors from 'ajv-errors';
import { formSchema, linksSchema } from "./constants";

const ajv = new Ajv2019({
  allErrors: true,
  removeAdditional: 'failing',
  useDefaults: true,
  discriminator: true,
  $data: true
});
ajvErrors(ajv);

export const validateAndFix = ajv.addSchema(linksSchema).compile(formSchema);
