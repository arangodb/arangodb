import React, { ReactNode } from 'react';

type GraphInfoProps = {
  children: ReactNode;
};

const GraphInfo = ({ children }: GraphInfoProps) => {

    return <>
        <div
            style={{
            'display': 'flex',
            'width': '97%',
            'margin': 'auto',
            'background': '#ffffff',
            'padding': '8px'
            }}>
                {children}
        </div>
    </>;
};

export default GraphInfo;
