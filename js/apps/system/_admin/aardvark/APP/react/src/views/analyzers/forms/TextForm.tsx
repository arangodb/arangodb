import React, { ChangeEvent, useEffect } from "react";
import { FormProps } from "../constants";
import CaseInput from "./CaseInput";
import { filter, get, has, isEmpty, negate } from 'lodash';

interface TextFormProps extends FormProps {
  unsetFormField: (field: string) => void;
}

const TextForm = ({ formState, updateFormField, unsetFormField }: TextFormProps) => {
  const updateLocale = (event: ChangeEvent<HTMLInputElement>) => {
    updateFormField('properties.locale', event.target.value);
  };

  const updateStopwords = (event: ChangeEvent<HTMLTextAreaElement>) => {
    const stopwords = event.target.value.split('\n');

    if (filter(stopwords, negate(isEmpty)).length) {
      updateFormField('properties.stopwords', stopwords);
    } else {
      unsetFormField('properties.stopwords');
    }
  };

  const getStopwords = () => {
    return get(formState, ['properties', 'stopwords'], []).join('\n');
  };

  const updateStopwordsPath = (event: ChangeEvent<HTMLInputElement>) => {
    const stopwordsPath = event.target.value;

    if (stopwordsPath) {
      updateFormField('properties.stopwordsPath', stopwordsPath);
    } else {
      unsetFormField('properties.stopwordsPath');
    }

  };

  const toggleAccent = () => {
    updateFormField('properties.accent', !get(formState, ['properties', 'accent']));
  };

  const toggleStemming = () => {
    updateFormField('properties.stemming', !get(formState, ['properties', 'stemming']));
  };

  useEffect(() => {
    if (!has(formState, ['properties', 'stemming'])) {
      updateFormField('properties.stemming', false);
    }

    if (!has(formState, ['properties', 'accent'])) {
      updateFormField('properties.accent', false);
    }
  }, [formState, updateFormField]);

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <div className={'pure-g'}>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'locale'}>Locale</label>
          <input id="locale" type="text" placeholder="language[_COUNTRY][.encoding][@variant]"
                 value={get(formState, ['properties', 'locale'], '')} onChange={updateLocale} required={true}
                 style={{
                   height: 'auto',
                   width: '90%'
                 }}/>
        </div>

        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'stemming'} className="pure-checkbox">
            <input id={'stemming'} type={'checkbox'}
                   checked={get(formState, ['properties', 'stemming'], false)}
                   onChange={toggleStemming} style={{ width: 'auto' }}/> Stemming
          </label>
        </div>

        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'accent'} className="pure-checkbox">
            <input id={'accent'} type={'checkbox'} checked={get(formState, ['properties', 'accent'], false)}
                   onChange={toggleAccent} style={{ width: 'auto' }}/> Accent
          </label>
        </div>

        <CaseInput formState={formState} updateFormField={updateFormField}/>
      </div>
    </div>

    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <div className={'pure-g'}>
        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'stopwordsPath'}>Stopwords Path</label>
          <input id="stopwordsPath" type="text"
                 value={get(formState, ['properties', 'stopwordsPath'], '')}
                 onChange={updateStopwordsPath}
                 style={{
                   height: 'auto',
                   width: '90%'
                 }}/>
        </div>

        <div className={'pure-u-1 pure-u-md-1 pure-u-lg-1 pure-u-xl-1'}>
          <label htmlFor={'stopwords'}>Stopwords (One per line)</label>
          <textarea id="stopwords" value={getStopwords()}
                    onChange={updateStopwords}
                    style={{
                      height: '80%',
                      width: '90%'
                    }}/>
        </div>
      </div>
    </div>
  </div>;
};

export default TextForm;
