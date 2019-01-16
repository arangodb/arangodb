//
// Copyright 2018 Mateusz Loskot <mateusz at loskot dot net>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil/concepts.hpp>
#include <boost/gil/image_view.hpp>
#include <boost/gil/typedefs.hpp>

namespace gil = boost::gil;

int main()
{
    boost::function_requires<gil::ImageViewConcept<gil::gray8_view_t>>();

    boost::function_requires<gil::CollectionImageViewConcept<gil::gray8_view_t>>();
    boost::function_requires<gil::ForwardCollectionImageViewConcept<gil::gray8_view_t>>();
    boost::function_requires<gil::ReversibleCollectionImageViewConcept<gil::gray8_view_t>>();

    return 0;
}
