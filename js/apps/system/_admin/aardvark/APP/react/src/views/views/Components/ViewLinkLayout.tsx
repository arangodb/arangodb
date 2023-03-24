import React from "react";
import { ArangoTable, ArangoTD, ArangoTH } from "../../../components/arango/table";
import { Link } from 'react-router-dom';
import { ChildProp } from "../../../utils/constants";

type ViewLinkLayoutProps = ChildProp & {
  fragments?: string[];
};

const ViewLinkLayout = ({ fragments = [], children }: ViewLinkLayoutProps) =>
  <div
    id={'modal-dialog'}
    className={'createModalDialog'}
    tabIndex={-1}
    role={'dialog'}
    aria-labelledby={'myModalLabel'}
    aria-hidden={'true'}
  >
    <div className="modal-body" style={{ overflowY: 'visible' }}>
      <div className={'tab-content'}>
        <div className="tab-pane tab-pane-modal active">
          <main>
            <ArangoTable>
              <thead>
              <tr className="figuresHeader">
                <ArangoTH seq={0} style={{ width: "100%" }}>
                  <ul className={'breadcrumb'}>
                    <li style={{
                      textShadow: 'none',
                      textDecoration: 'underline'
                    }}>
                      <Link to={'/'}>Links</Link>
                      <span className="divider">
                        <i className={'fa fa-angle-double-right'}/>
                      </span>
                    </li>
                    {
                      fragments.slice(0, fragments.length - 1).map((fragment, idx) => {
                        const path = fragments.slice(0, idx + 1).join('/');

                        return <li key={`${idx}-${fragment}`} style={{
                          textShadow: 'none',
                          textDecoration: 'underline',
                          display: 'inline-flex'
                        }}>
                          <Link
                            style={{
                              maxWidth: "200px",
                              whiteSpace: 'nowrap',
                              textOverflow: 'ellipsis',
                              overflow: 'hidden',
                              display: 'inline-block'
                            }} 
                            to={`/${path}`}
                            title={fragment}>
                              {fragment}
                            </Link>
                          <span className="divider">
                            <i className={'fa fa-angle-double-right'}/>
                          </span>
                        </li>;
                      })
                    }
                    <li style={{
                      textShadow: 'none',
                      maxWidth: "200px",
                      whiteSpace: 'nowrap',
                      textOverflow: 'ellipsis',
                      overflow: 'hidden',
                      verticalAlign: 'middle'
                    }}
                    title={fragments[fragments.length - 1]}
                    >{fragments[fragments.length - 1]}</li>
                  </ul>
                </ArangoTH>
              </tr>
              </thead>
              <tbody>
              <tr>
                <ArangoTD seq={0}>{children}</ArangoTD>
              </tr>
              </tbody>
            </ArangoTable>
          </main>
        </div>
      </div>
    </div>
  </div>;

export default ViewLinkLayout;
