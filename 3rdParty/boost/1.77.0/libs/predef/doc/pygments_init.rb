=begin
Copyright 2018 Rene Rivera
Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE.txt or http://www.boost.org/LICENSE_1_0.txt)
=end

require 'pygments'

# Need to create/register non-builtin lexers even if they are global plugins.
Pygments::Lexer.create name: 'Jam', aliases: ['jam','bjam','b2'],
    filenames: ['*.jam','Jamfile','Jamroot'], mimetypes: ['text/x-jam']
