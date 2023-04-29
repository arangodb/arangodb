import { Box, ListItem, UnorderedList } from '@chakra-ui/react';
import Ajv2019 from "ajv/dist/2019";
import { JsonEditor as Editor } from 'jsoneditor-react';
import { cloneDeep, pick } from "lodash";
import React, { useEffect, useRef, useState } from 'react';
import { FormProps, State } from "../../../utils/constants";
import { useJsonFormErrorHandler, useJsonFormUpdateEffect } from "../../../utils/helpers";
import { formSchema, FormState, linksSchema } from "../constants";

const ajv = new Ajv2019({
  allErrors: true,
  verbose: true,
  discriminator: true,
  $data: true
});
ajv.addSchema(linksSchema);

type JsonFormProps =
  Pick<FormProps<FormState>, 'formState' | 'dispatch'>
  & Pick<State<FormState>, 'renderKey'>
  & {
  isEdit?: boolean;
  mode?: 'code' | 'view'
};

const JsonForm = ({ formState, dispatch, renderKey, mode = 'code', isEdit = false }: JsonFormProps) => {
  const [formErrors, setFormErrors] = useState<string[]>([]);
  const validate = useRef(ajv.compile(formSchema));
  const formStateSchema = useRef(cloneDeep(formSchema));
  const prevId = useRef('');
  const raiseError = useJsonFormErrorHandler(dispatch, setFormErrors);

  useEffect(() => {
    if (formState.id && formState.id !== prevId.current) {
      prevId.current = formState.id;

      const fss = formStateSchema.current;

      fss.$id = `https://arangodb.com/schemas/views/views-${formState.id}.json`;
      fss.properties.id = {
        const: formState.id
      };
      fss.properties.globallyUniqueId = {
        const: formState.globallyUniqueId
      };
      fss.properties.primarySort = {
        const: formState.primarySort
      };
      fss.properties.primarySortCompression = {
        const: formState.primarySortCompression
      };
      fss.properties.storedValues = {
        const: formState.storedValues
      };
      fss.properties.writebufferIdle = {
        const: formState.writebufferIdle
      };
      fss.properties.writebufferActive = {
        const: formState.writebufferActive
      };
      fss.properties.writebufferSizeMax = {
        const: formState.writebufferSizeMax
      };
      if (window.frontendConfig.isCluster) {
        fss.properties.name = {
          const: formState.name
        };
      }

      validate.current = ajv.compile(fss);
    }
  }, [formState.globallyUniqueId, formState.id, formState.name, formState.primarySort, formState.primarySortCompression, formState.storedValues, formState.writebufferActive, formState.writebufferIdle, formState.writebufferSizeMax, prevId]);

  useEffect(() => {
    let fssid = '';

    if (formState.id) {
      fssid = formStateSchema.current.$id as string;
    }

    return () => {
      if (fssid) {
        ajv.removeSchema(fssid);
      }
    };
  }, [formState.id]);

  useJsonFormUpdateEffect(validate.current, formState, raiseError, setFormErrors);

  const changeHandler = (json: FormState) => {
    const validateFunc = validate.current;
    if (validateFunc(json)) {
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
    } else if (Array.isArray(validateFunc.errors)) {
      dispatch({ type: 'lockJsonForm' });

      setFormErrors(validateFunc.errors.map(error =>
        `
          ${error.keyword} error: ${error.instancePath} ${error.message}.
          Schema: ${JSON.stringify(error.params)}
        `
      ));
    }
  };

  return (
    <Box display="grid" gridTemplateRows="1fr 80px" height="full">
      <Editor
        value={formState}
        onChange={changeHandler}
        mode={mode}
        history={true}
        key={renderKey}
        htmlElementProps={{ style: { height: "100%" } }}
      />

      {formErrors.length ? (
        <UnorderedList paddingLeft="4" color="red.600" overflow="auto">
          {formErrors.map((error, idx) => (
            <ListItem key={idx} style={{ marginBottom: 5 }}>
              {error}
            </ListItem>
          ))}
        </UnorderedList>
      ) : null}
    </Box>
  );
};

export default JsonForm;
