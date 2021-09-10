import { JsonEditor as Editor } from 'jsoneditor-react';
import React, { useCallback, useEffect, useState } from 'react';
import Ajv, { ErrorObject } from 'ajv';
import { FormProps, formSchema, FormState, State } from "../constants";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { usePrevious } from "../../../utils/helpers";
import { has, isEqual } from 'lodash';
import ajvErrors from 'ajv-errors';

const ajv = new Ajv({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});
ajvErrors(ajv);
const validate = ajv.compile(formSchema);

type JsonFormProps = Pick<FormProps, 'formState' | 'dispatch'> & Pick<State, 'renderKey'>;

const JsonForm = ({ formState, dispatch, renderKey }: JsonFormProps) => {
  const [formErrors, setFormErrors] = useState<string[]>([]);
  const prevFormState = usePrevious(formState);

  const raiseError = useCallback((errors: ErrorObject[] | null | undefined) => {
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
  }, [dispatch]);

  const changeHandler = (json: FormState) => {
    if (validate(json)) {
      dispatch({
        type: 'setFormState',
        formState: json
      });
      dispatch({ type: 'unlockJsonForm' });
      setFormErrors([]);
    } else {
      raiseError(validate.errors);
    }
  };

  useEffect(() => {
    if (!isEqual(prevFormState, formState) && !validate(formState)) {
      raiseError(validate.errors);
    }
  }, [formState, prevFormState, raiseError]);

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
