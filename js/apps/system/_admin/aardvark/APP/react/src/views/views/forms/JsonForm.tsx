import { JsonEditor as Editor } from 'jsoneditor-react';
import React, { useState } from 'react';
import Ajv2019 from "ajv/dist/2019";
import { formSchema, FormState, linksSchema } from "../constants";
import { FormProps, State } from "../../../utils/constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { pick } from "lodash";

const ajv = new Ajv2019({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});
const validate = ajv.addSchema(linksSchema).compile(formSchema);

type JsonFormProps =
  Pick<FormProps<FormState>, 'formState' | 'dispatch'>
  & Pick<State<FormState>, 'renderKey'>
  & {
  isEdit?: boolean
};

const JsonForm = ({ formState, dispatch, renderKey, isEdit = false }: JsonFormProps) => {
  const [formErrors, setFormErrors] = useState<string[]>([]);

  const changeHandler = (json: FormState) => {
    if (validate(json)) {
      if (isEdit) {
        json = Object.assign({}, formState, pick(json, 'consolidationIntervalMsec', 'commitIntervalMsec',
          'cleanupIntervalStep', 'links', 'consolidationPolicy'));
      }

      dispatch({
        type: 'setFormState',
        formState: json
      });
      dispatch({ type: 'unlockJsonForm' });
      setFormErrors([]);
    } else if (Array.isArray(validate.errors)) {
      dispatch({ type: 'lockJsonForm' });

      setFormErrors(validate.errors.map(error =>
        `
          ${error.keyword} error: ${error.instancePath} ${error.message}.
          Schema: ${JSON.stringify(error.params)}
        `
      ));
    }
  };

  return <Grid>
    <Cell size={'1'}>
      <Editor value={formState} onChange={changeHandler} mode={'code'} history={true} key={renderKey}/>
    </Cell>
    {
      formErrors.length
        ? <Cell size={'1'} style={{ marginTop: 35 }}>
          <ul style={{ color: 'red' }}>
            {formErrors.map((error, idx) => <li key={idx} style={{ marginBottom: 5 }}>{error}</li>)}
          </ul>
        </Cell>
        : null
    }
  </Grid>;
};

export default JsonForm;
