/* global arangoHelper, frontendConfig */

import minimatch from 'minimatch';
import React, { useEffect, useState } from 'react';
import { isEqual } from 'underscore';
import useAPIFetch from '../../utils/arangoSWRClient';

const AnalyzersReactView = () => {
  const {
    data,
    error
  } = useAPIFetch('/analyzer');
  const {
    data: permData,
    error: permError
  } = useAPIFetch(`/user/${arangoHelper.getCurrentJwtUsername()}/database/${frontendConfig.db}`);
  const [filterExpr, setFilterExpr] = useState('');
  const [filteredAnalyzers, setFilteredAnalyzers] = useState([]);
  const [analyzers, setAnalyzers] = useState([]);

  useEffect(() => {
    if (filterExpr) {
      let tempFilteredAnalyzers = analyzers;

      try {
        const filters = filterExpr.trim().split(/\s+/);

        for (const filter of filters) {
          const [field, pattern] = filter.split(':');

          tempFilteredAnalyzers = tempFilteredAnalyzers.filter(
            analyzer => minimatch(analyzer[field].toLowerCase(), `*${pattern.toLowerCase()}*`));
        }
      } catch (e) {
        tempFilteredAnalyzers = tempFilteredAnalyzers.filter(analyzer => {
          const normalizedPattern = `*${filterExpr.toLowerCase()}*`;

          return ['name', 'type'].some(
            field => minimatch(analyzer[field].toLowerCase(), normalizedPattern));
        });
      }

      setFilteredAnalyzers(tempFilteredAnalyzers);
    } else {
      setFilteredAnalyzers(analyzers);
    }
  }, [analyzers, filterExpr]);

  function handleChange (event) {
    setFilterExpr(event.target.value);
  }

  if (!error && data && !data.body.error && !permError && permData && !permData.body.error) {
    const permission = permData.body.result;

    if (!isEqual(data.body.result, analyzers)) {
      setAnalyzers(data.body.result);
      setFilteredAnalyzers(data.body.result);
    }

    return <div className={'innerContent'} id={'analyzersContent'} style={{ paddingTop: 0 }}>
      <div className={'pure-g'}>
        <div className={'pure-u-1-1 pure-u-md-1-1 pure-u-lg-1-1 pure-u-xl-1-1'}>
          <div className={'sectionHeader pure-g'}>
            <div className={'pure-u-2-5'}>
              <div className={'title'}>List of Analyzers</div>
            </div>

            <div className={'pure-u-3-5'}>
              <span className={'search-field'} style={{
                marginRight: 10,
                whiteSpace: 'nowrap'
              }}>
                <label htmlFor={'filterInput'} style={{
                  color: '#fff',
                  display: 'inline'
                }}>Filter: </label>
                <input type={'text'} id={'filterInput'} className={'search-input'}
                       value={filterExpr} onChange={handleChange}
                       placeholder={'<glob> | (<name|type>:<glob> )+'}
                       style={{
                         marginRight: 0,
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
              </span>
            </div>
          </div>
          <table className={'arango-table'}>
            <thead>
            <tr>
              <th className={'arango-table-th table-cell0'}>Name</th>
              <th className={'arango-table-th table-cell1'}>Type</th>
              <th className={'arango-table-th table-cell2'}>Actions</th>
            </tr>
            </thead>
            <tbody>
            {filteredAnalyzers.map((analyzer, idx) => (
              <tr key={idx}>
                <td className={'arango-table-td table-cell0'}>{analyzer.name}</td>
                <td className={'arango-table-td table-cell1'}>{analyzer.type}</td>
                <td className={'arango-table-td table-cell2'}>
                  <a href={`#analyzers/view/${analyzer.name}`}><i className={'fa fa-eye'}/></a>
                  &nbsp;
                  {
                    permission === 'rw' && analyzer.name.includes('::')
                      ? <a href={`#analyzers/remove/${analyzer.name}`}>
                        <i className={'fa fa-trash-o'}/>
                      </a>
                      : null
                  }
                </td>
              </tr>
            ))}
            </tbody>
          </table>
        </div>
      </div>
    </div>;
  }

  return <h1>Analyzers</h1>;
};

window.AnalyzersReactView = AnalyzersReactView;
