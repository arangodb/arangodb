import { FormProps } from "../../../../utils/constants";
import { StoredValue } from "../../constants";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import React, { ChangeEvent } from "react";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { IconButton } from "../../../../components/arango/buttons";

const StoredValueFieldsInput = ({ formState, dispatch, disabled }: FormProps<StoredValue>) => {
  const items = formState.fields;

  const removeField = (index: number) => {
    const tempItems = items.slice();

    tempItems.splice(index, 1);
    dispatch({
      type: 'setField',
      field: {
        path: 'fields',
        value: tempItems
      }
    });
  };

  const getRemover = (index: number) => () => {
    removeField(index);
  };

  const moveUp = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const prevItem = tempItems[index - 1];

    tempItems.splice(index - 1, 2, item, prevItem);
    dispatch({
      type: 'setField',
      field: {
        path: 'fields',
        value: tempItems
      }
    });
  };

  const moveDown = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const nextItem = tempItems[index + 1];

    tempItems.splice(index, 2, nextItem, item);
    dispatch({
      type: 'setField',
      field: {
        path: 'fields',
        value: tempItems
      }
    });
  };

  const getShifter = (direction: 'up' | 'down', index: number) => {
    switch (direction) {
      case 'up':
        return () => {
          moveUp(index);
        };
      case 'down':
        return () => {
          moveDown(index);
        };
    }
  };

  const addField = () => {
    dispatch({
      type: 'setField',
      field: {
        path: `fields[${items.length}]`,
        value: ''
      }
    });
  };

  return <ArangoTable style={{ marginLeft: 0 }}>
    {
      disabled
        ? null
        : <thead>
        <tr>
          <ArangoTH seq={0} style={{ width: '75%' }}/>
          <ArangoTH seq={1} style={{ width: '25%' }}>
            <IconButton icon={'plus'} type={'warning'} onClick={addField}>Add Field</IconButton>
          </ArangoTH>
        </tr>
        </thead>
    }
    <tbody>
    {
      items.length
        ? items.map((item, idx) => {
          const isFirst = idx === 0;
          const isLast = idx === items.length - 1;

          const updateField = (event: ChangeEvent<HTMLInputElement>) => {
            dispatch(
              {
                type: 'setField',
                field: {
                  path: `fields[${idx}]`,
                  value: event.target.value
                }
              }
            );
          };

          return <tr key={idx}>
            <ArangoTD seq={0}>
              <Textbox type={'text'} value={item} onChange={updateField} disabled={disabled}/>
            </ArangoTD>
            {
              disabled
                ? null
                : <ArangoTD seq={1}>
                  <IconButton icon={'trash-o'} type={'danger'} onClick={getRemover(idx)}/>&nbsp;
                  <IconButton icon={'arrow-up'} type={'warning'} onClick={getShifter('up', idx)}
                              disabled={isFirst}/>&nbsp;
                  <IconButton icon={'arrow-down'} type={'warning'} onClick={getShifter('down', idx)}
                              disabled={isLast}/>
                </ArangoTD>
            }
          </tr>;
        })
        : <tr>
          <ArangoTD seq={0} colSpan={disabled ? 1 : 2}>No fields found.</ArangoTD>
        </tr>
    }
    </tbody>
  </ArangoTable>;
};

export default StoredValueFieldsInput;
