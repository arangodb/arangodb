import React from 'react';

type TagProps = {
  label?: string;
  template?: string;
};

const Tag = ({ label, template }: TagProps) => {

    if (template === 'transparent') {
        return <>
            {label ?
                <div
                    style={{
                        'background': 'transparent',
                        'color': '#333333',
                        'padding': '4px 0px',
                        'borderRadius': '4px',
                        'width': 'fit-content',
                        'margin': '4px'
                        }}
                >
                    {label}
                </div> :
                null
            }
        </>;
    }

    return <>
        {label ?
            <div
                style={{
                    'background': '#404A53',
                    'color': '#ffffff',
                    'padding': '4px 8px',
                    'borderRadius': '4px',
                    'width': 'fit-content',
                    'margin': '4px'
                    }}
            >
                {label}
            </div> :
            null
        }
    </>;
};

export default Tag;
