/* global arangoHelper, frontendConfig */

import minimatch from 'minimatch';
import React, { useEffect, useState } from 'react';
import useSWR from 'swr';
import { compact, get, isEqual } from 'underscore';
import { getApiRouteForCurrentDB } from '../../utils/arangoClient';
import { getChangeHandler } from '../../utils/helpers';
import Actions from './Actions';
import AddAnalyzer from './AddAnalyzer';
import { typeNameMap } from './constants';


const AnalyzersReactView = () => {
  const {
    data,
    error
  } = useSWR('/analyzer', (path) => getApiRouteForCurrentDB().get(path));

  const {
    data: permData,
    error: permError
  } = useSWR(`/user/${arangoHelper.getCurrentJwtUsername()}/database/${frontendConfig.db}`,
    (path) => getApiRouteForCurrentDB().get(path));

  const [filterExpr, setFilterExpr] = useState('');
  const [filteredAnalyzers, setFilteredAnalyzers] = useState([]);
  const [analyzers, setAnalyzers] = useState([]);

  function processAndSetFilteredAnalyzers (analyzers) {
    analyzers.forEach(analyzer => {
      analyzer.db = analyzer.name.includes('::') ? analyzer.name.split('::')[0] : '_system';
    });

    setFilteredAnalyzers(analyzers);
  }

  useEffect(() => {
    let tempFilteredAnalyzers = analyzers;

    if (filterExpr) {
      try {
        const filters = filterExpr.trim().split(/\s+/);

        for (const filter of filters) {
          const splitIndex = filter.indexOf(':');
          const field = filter.slice(0, splitIndex);
          const pattern = filter.slice(splitIndex + 1);

          tempFilteredAnalyzers = tempFilteredAnalyzers.filter(
            analyzer => minimatch(analyzer[field].toLowerCase(), `*${pattern.toLowerCase()}*`));
        }
      } catch (e) {
        tempFilteredAnalyzers = tempFilteredAnalyzers.filter(analyzer => {
          const normalizedPattern = `*${filterExpr.toLowerCase()}*`;

          return ['db', 'name', 'type'].some(
            field => minimatch(analyzer[field].toLowerCase(), normalizedPattern));
        });
      }
    }

    processAndSetFilteredAnalyzers(tempFilteredAnalyzers);
  }, [analyzers, filterExpr]);

  const errMsgs = compact([
    get(error, 'message'),
    get(permError, 'message'),
    get(data, ['body', 'errorMessage']),
    get(permData, ['body', 'errorMessage'])
  ]);

  for (const emsg in errMsgs) {
    arangoHelper.arangoError('Failure', `Got unexpected server response: ${emsg}`);
  }

  if (!errMsgs.length && data && permData) {
    const permission = permData.body.result;

    if (!isEqual(data.body.result, analyzers)) {
      setAnalyzers(data.body.result);
      processAndSetFilteredAnalyzers(data.body.result);
    }

    return <div className={'innerContent'} id={'analyzersContent'} style={{ paddingTop: 0 }}>
      <div className={'pure-g'}>
        <div className={'pure-u-1-1 pure-u-md-1-1 pure-u-lg-1-1 pure-u-xl-1-1'}>
          <div className={'sectionHeader pure-g'}>
            <div className={'pure-u-2-5'}>
              <div className={'title'}><AddAnalyzer/></div>
            </div>
          </div>
        </div>
        <div className={'pure-u-1-1 pure-u-md-1-1 pure-u-lg-1-1 pure-u-xl-1-1'}>
          <div className={'sectionHeader pure-g'}>
            <div className={'pure-u-2-5'}>
              <div className={'title'}>List of Analyzers</div>
            </div>

            <div className={'pure-u-3-5'}>
              <label htmlFor={'filterInput'} style={{
                color: '#fff',
                marginRight: 10,
                float: 'right'
              }}>
                Filter: <input type={'text'} id={'filterInput'} className={'search-input'}
                               value={filterExpr} onChange={getChangeHandler(setFilterExpr)}
                               placeholder={'<glob> | (<db|name|type>:<glob> )+'}
                               style={{
                                 margin: 0,
                                 width: 300,
                                 paddingLeft: 25
                               }}/>
                <i className={'fa fa-search'} style={{
                  position: 'relative',
                  float: 'left',
                  top: 10,
                  left: 65,
                  cursor: 'default'
                }}/>
              </label>
            </div>
          </div>
          <table className={'arango-table'}>
            <thead>
            <tr>
              <th className={'arango-table-th table-cell0'}>DB</th>
              <th className={'arango-table-th table-cell1'}>Name</th>
              <th className={'arango-table-th table-cell2'}>Type</th>
              <th className={'arango-table-th table-cell3'}>Actions</th>
            </tr>
            </thead>
            <tbody>
            {
              filteredAnalyzers.map((analyzer, idx) => (
                <tr key={idx}>
                  <td className={'arango-table-td table-cell0'}>{analyzer.db}</td>
                  <td className={'arango-table-td table-cell1'}>{analyzer.name}</td>
                  <td className={'arango-table-td table-cell2'}>{typeNameMap[analyzer.type]}</td>
                  <td className={'arango-table-td table-cell3'}>
                    <Actions analyzer={analyzer} permission={permission}/>
                  </td>
                </tr>
              ))
            }
            </tbody>
          </table>
        </div>

      </div>
    </div>;
  }

  return <h1>Analyzers</h1>;
};

window.AnalyzersReactView = AnalyzersReactView;
