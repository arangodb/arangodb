import { DispatchArgs, FormProps } from "../../../../utils/constants";
import React, { ChangeEvent, Dispatch } from "react";
import Fieldset from "../../../../components/pure-css/form/Fieldset";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../../components/arango/table";
import Textbox from "../../../../components/pure-css/form/Textbox";
import { getPath } from "../../../../utils/helpers";
import Select from "../../../../components/pure-css/form/Select";
import styled from "styled-components";
import { PrimarySort } from "../../constants";

const StyledIcon = styled.i`
  &&& {
    margin-left: auto;
  }
`;

const columnWidths: { [key: string]: number[] } = {
  'false': [5, 60, 10, 25],
  'true': [5, 85, 10, 0]
};

const PrimarySortInput = ({ formState, dispatch, disabled }: FormProps<PrimarySort>) => {
  const items = formState.primarySort || [];

  const removeItem = (index: number) => {
    const tempItems = items.slice();

    tempItems.splice(index, 1);
    dispatch({
      type: 'setField',
      field: {
        path: 'primarySort',
        value: tempItems
      }
    });
  };

  const getRemover = (index: number) => () => {
    removeItem(index);
  };

  const moveUp = (index: number) => {
    const tempItems = items.slice();
    const item = tempItems[index];
    const prevItem = tempItems[index - 1];

    tempItems.splice(index - 1, 2, item, prevItem);
    dispatch({
      type: 'setField',
      field: {
        path: 'primarySort',
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
        path: 'primarySort',
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

  const addPrimarySort = () => {
    const primarySort = {
      field: '',
      direction: 'asc'
    };

    dispatch({
      type: 'setField',
      field: {
        path: `primarySort[${items.length}]`,
        value: primarySort
      }
    });
  };

  const getWrappedDispatch = (index: number): Dispatch<DispatchArgs<PrimarySort>> => (action: DispatchArgs<PrimarySort>) => {
    action.basePath = getPath(`primarySort[${index}]`, action.basePath);
    (dispatch as unknown as Dispatch<DispatchArgs<PrimarySort>>)(action);
  };

  const getColumnWidth = (index: number) => {
    const key = `${!!disabled}`;

    return `${columnWidths[key][index]}%`;
  };

  return <Fieldset legend={'Primary Sort'}>
    <ArangoTable>
      <thead>
      <tr>
        <ArangoTH seq={0} style={{ width: getColumnWidth(0) }}>#</ArangoTH>
        <ArangoTH seq={1} style={{ width: getColumnWidth(1) }}>Field</ArangoTH>
        <ArangoTH seq={2} style={{ width: getColumnWidth(2) }}>Direction</ArangoTH>
        {
          disabled
            ? null
            : <ArangoTH seq={3} style={{ width: getColumnWidth(3) }}>
              <button className={'button-warning'} onClick={addPrimarySort}>
                <i className={'fa fa-plus'}/>&nbsp;Add Primary Sort
              </button>
            </ArangoTH>
        }
      </tr>
      </thead>
      <tbody>
      {
        items.length
          ? items.map((item, idx) => {
            const isLast = idx === items.length - 1;
            const itemDispatch = getWrappedDispatch(idx);

            const updateField = (event: ChangeEvent<HTMLInputElement>) => {
              itemDispatch(
                {
                  type: 'setField',
                  field: {
                    path: 'field',
                    value: event.target.value
                  }
                }
              );
            };

            const updateDirection = (event: ChangeEvent<HTMLSelectElement>) => {
              itemDispatch(
                {
                  type: 'setField',
                  field: {
                    path: 'direction',
                    value: event.target.value
                  }
                }
              );
            };

            return <tr key={idx} style={{ borderBottom: '1px  solid #DDD' }}>
              <ArangoTD seq={0} valign={'middle'}>{idx + 1}.</ArangoTD>
              <ArangoTD seq={1}>
                <Textbox type={'text'} value={item.field} onChange={updateField}
                         disabled={disabled}/>
              </ArangoTD>
              <ArangoTD seq={2}>
                <Select value={item.direction || 'asc'} onChange={updateDirection} disabled={disabled}>
                  <option key={'asc'} value={'asc'}>ASC</option>
                  <option key={'desc'} value={'desc'}>DESC</option>
                </Select>
              </ArangoTD>
              {
                disabled
                  ? null
                  : <ArangoTD seq={3}>
                    <button className={'button-danger'} onClick={getRemover(idx)}>
                      <StyledIcon className={'fa fa-trash-o'}/>
                    </button>
                    &nbsp;
                    <button className={'button-warning'} onClick={getShifter('up', idx)}
                            disabled={idx === 0}>
                      <StyledIcon className={'fa fa-arrow-up'}/>
                    </button>
                    &nbsp;
                    <button className={'button-warning'} onClick={getShifter('down', idx)}
                            disabled={isLast}>
                      <StyledIcon className={'fa fa-arrow-down'}/>
                    </button>
                  </ArangoTD>
              }
            </tr>;
          })
          : <tr>
            <ArangoTD seq={0} colSpan={disabled ? 3 : 4}>No primary sort definitions found.</ArangoTD>
          </tr>
      }
      </tbody>
    </ArangoTable>
  </Fieldset>;
};

export default PrimarySortInput;
