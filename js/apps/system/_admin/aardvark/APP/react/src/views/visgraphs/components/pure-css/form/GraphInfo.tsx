import React, { ReactNode } from 'react';

type GraphInfoProps = {
  children: ReactNode;
};

const GraphInfo = ({ children }: GraphInfoProps) => {

    return <>
        <div
            style={{
            'display': 'flex',
            'width': '100%',
            'background': '#ffffff',
            'padding': '8px'
            }}>
                {children}
        </div>
    </>;
};

export default GraphInfo;
