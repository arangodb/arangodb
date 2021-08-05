import { JsonEditor as Editor } from 'jsoneditor-react';
import React from 'react';

declare var ace: any;

interface JsonFromProps {
  formState: { [key: string]: any };
  setFormState: Function;
}

const JsonForm = ({ formState, setFormState }: JsonFromProps) =>
  <div style={{ overflow: 'auto' }} className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
    <Editor value={formState} onChange={setFormState} ace={ace} mode={'tree'} history={true}
            allowedModes={['code', 'tree']} navigationBar={false}/>
  </div>;

export default JsonForm;
