import React from 'react';

export const SearchBox = ({ placeholder, searchChange }) => (
    <input
        className='search'
        type='search'
        placeholder={placeholder}
        onChange={searchChange} />
);
