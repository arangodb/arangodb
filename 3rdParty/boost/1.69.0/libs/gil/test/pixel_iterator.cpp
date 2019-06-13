//
// Copyright 2005-2007 Adobe Systems Incorporated
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#include <boost/gil.hpp>

#include <boost/mpl/vector.hpp>

#include <cassert>
#include <exception>
#include <iostream>
#include <vector>

using namespace boost::gil;
using namespace std;

void test_pixel_iterator()
{
    boost::function_requires<Point2DConcept<point<int>>>();

    boost::function_requires<MutablePixelIteratorConcept<bgr8_ptr_t> >();
    boost::function_requires<MutablePixelIteratorConcept<cmyk8_planar_ptr_t> >();
    boost::function_requires<PixelIteratorConcept<rgb8c_planar_step_ptr_t> >();
    boost::function_requires<MutableStepIteratorConcept<rgb8_step_ptr_t> >();

    boost::function_requires<MutablePixelLocatorConcept<rgb8_step_loc_t> >();
    boost::function_requires<PixelLocatorConcept<rgb8c_planar_step_loc_t> >();

    boost::function_requires<MutableStepIteratorConcept<cmyk8_planar_step_ptr_t> >();
    boost::function_requires<StepIteratorConcept<gray8c_step_ptr_t> >();

    boost::function_requires<MutablePixelLocatorConcept<memory_based_2d_locator<rgb8_step_ptr_t> > >();

    typedef const bit_aligned_pixel_reference<std::uint8_t, boost::mpl::vector3_c<int,1,2,1>, bgr_layout_t, true>  bgr121_ref_t;
    typedef bit_aligned_pixel_iterator<bgr121_ref_t> bgr121_ptr_t;

    boost::function_requires<MutablePixelIteratorConcept<bgr121_ptr_t> >();
    boost::function_requires<PixelBasedConcept<bgr121_ptr_t> >();
    boost::function_requires<MemoryBasedIteratorConcept<bgr121_ptr_t> >();
    boost::function_requires<HasDynamicXStepTypeConcept<bgr121_ptr_t> >();

// TEST dynamic_step_t
    BOOST_STATIC_ASSERT(( boost::is_same<cmyk16_step_ptr_t,dynamic_x_step_type<cmyk16_step_ptr_t>::type>::value ));
    BOOST_STATIC_ASSERT(( boost::is_same<cmyk16_planar_step_ptr_t,dynamic_x_step_type<cmyk16_planar_ptr_t>::type>::value ));

    BOOST_STATIC_ASSERT(( boost::is_same<iterator_type<uint8_t,gray_layout_t,false,false,false>::type,gray8c_ptr_t>::value ));

// TEST iterator_is_step
    BOOST_STATIC_ASSERT(iterator_is_step< cmyk16_step_ptr_t >::value);
    BOOST_STATIC_ASSERT(iterator_is_step< cmyk16_planar_step_ptr_t >::value);
    BOOST_STATIC_ASSERT(!iterator_is_step< cmyk16_planar_ptr_t >::value);

    typedef color_convert_deref_fn<rgb8c_ref_t, gray8_pixel_t> ccv_rgb_g_fn;
    typedef color_convert_deref_fn<gray8c_ref_t, rgb8_pixel_t> ccv_g_rgb_fn;
    gil_function_requires<PixelDereferenceAdaptorConcept<ccv_rgb_g_fn> >();
    gil_function_requires<PixelDereferenceAdaptorConcept<deref_compose<ccv_rgb_g_fn,ccv_g_rgb_fn> > >();

    typedef dereference_iterator_adaptor<rgb8_ptr_t, ccv_rgb_g_fn> rgb2gray_ptr;
    BOOST_STATIC_ASSERT(!iterator_is_step< rgb2gray_ptr >::value);

    typedef dynamic_x_step_type<rgb2gray_ptr>::type rgb2gray_step_ptr;
    BOOST_STATIC_ASSERT(( boost::is_same< rgb2gray_step_ptr, dereference_iterator_adaptor<rgb8_step_ptr_t, ccv_rgb_g_fn> >::value));

    make_step_iterator(rgb2gray_ptr(),2);

    typedef dereference_iterator_adaptor<rgb8_step_ptr_t, ccv_rgb_g_fn> rgb2gray_step_ptr1;
    BOOST_STATIC_ASSERT(iterator_is_step< rgb2gray_step_ptr1 >::value);
    BOOST_STATIC_ASSERT(( boost::is_same< rgb2gray_step_ptr1, dynamic_x_step_type<rgb2gray_step_ptr1>::type >::value));

    typedef memory_based_step_iterator<dereference_iterator_adaptor<rgb8_ptr_t, ccv_rgb_g_fn> > rgb2gray_step_ptr2;
    BOOST_STATIC_ASSERT(iterator_is_step< rgb2gray_step_ptr2 >::value);
    BOOST_STATIC_ASSERT(( boost::is_same< rgb2gray_step_ptr2, dynamic_x_step_type<rgb2gray_step_ptr2>::type >::value));
    make_step_iterator(rgb2gray_step_ptr2(),2);

// bit_aligned iterators test

    // Mutable reference to a BGR232 pixel
    typedef const bit_aligned_pixel_reference<std::uint8_t, boost::mpl::vector3_c<unsigned,2,3,2>, bgr_layout_t, true>  bgr232_ref_t;

    // A mutable iterator over BGR232 pixels
    typedef bit_aligned_pixel_iterator<bgr232_ref_t> bgr232_ptr_t;

    // BGR232 pixel value. It is a packed_pixel of size 1 byte. (The last bit is unused)
    typedef std::iterator_traits<bgr232_ptr_t>::value_type bgr232_pixel_t;
    BOOST_STATIC_ASSERT((sizeof(bgr232_pixel_t)==1));

    bgr232_pixel_t red(0,0,3); // = 0RRGGGBB, = 01100000

    // a buffer of 7 bytes fits exactly 8 BGR232 pixels.
    unsigned char pix_buffer[7];
    std::fill(pix_buffer,pix_buffer+7,0);
    bgr232_ptr_t pix_it(&pix_buffer[0],0);  // start at bit 0 of the first pixel
    for (int i=0; i<8; ++i) {
        *pix_it++ = red;
    }

	// test cross byte pixel values - meaning when a pixel value is stretched over two bytes
	typedef bit_aligned_image1_type< 3, gray_layout_t >::type gray3_image_t;
    typedef gray3_image_t image_t;

    typedef image_t::view_t view_t;
    typedef view_t::reference ref_t;

    typedef bit_aligned_pixel_iterator< ref_t > iterator_t;

	std::vector< unsigned char > buf( 4 );
	// bit pattern is: 1011 0110  0110 1101  1101 1011
	// each byte is read right to left
	buf[0] = 182;
    buf[1] = 109;
    buf[2] = 219;

    iterator_t it( &buf[0], 0 );

    ref_t p1 = *it; it++;
    ref_t p2 = *it; it++;
    ref_t p3 = *it; it++;
    ref_t p4 = *it; it++;
    ref_t p5 = *it; it++;
    ref_t p6 = *it; it++;
    ref_t p7 = *it; it++;
    ref_t p8 = *it; it++;

    unsigned char v1 = get_color( p1, gray_color_t() );
    unsigned char v2 = get_color( p2, gray_color_t() );
    unsigned char v3 = get_color( p3, gray_color_t() );
    unsigned char v4 = get_color( p4, gray_color_t() );
    unsigned char v5 = get_color( p5, gray_color_t() );
    unsigned char v6 = get_color( p6, gray_color_t() );
    unsigned char v7 = get_color( p7, gray_color_t() );
    unsigned char v8 = get_color( p8, gray_color_t() );

	// all values should be 110b ( 6 );
    assert( v1 == 6 );
    assert( v2 == 6 );
    assert( v3 == 6 );
    assert( v4 == 6 );
    assert( v5 == 6 );
    assert( v6 == 6 );
    assert( v7 == 6 );
    assert( v8 == 6 );
}

// TODO: Make better tests. Use some code from below.

/*
template <typename Pixel>
void invert_pixel1(Pixel& pix) {
    at_c<0>(pix)=0;
}

template <typename T> inline void ignore_unused_variable_warning(const T&){}

void test_pixel_iterator() {
    rgb8_pixel_t rgb8(1,2,3);
    rgba8_pixel_t rgba8;

    rgb8_ptr_t ptr1=&rgb8;
    memunit_advance(ptr1, 3);
    const rgb8_ptr_t ptr2=memunit_advanced(ptr1,10);

    memunit_distance(ptr1,ptr2);
    const rgb8_pixel_t& ref=memunit_advanced_ref(ptr1,10); ignore_unused_variable_warning(ref);

    rgb8_planar_ptr_t planarPtr1(&rgb8);
    rgb8_planar_ptr_t planarPtr2(&rgb8);
    memunit_advance(planarPtr1,10);
    memunit_distance(planarPtr1,planarPtr2);
    rgb8_planar_ptr_t planarPtr3=memunit_advanced(planarPtr1,10);

//    planarPtr2=&rgba8;

    planar_pixel_reference<uint8_t&,rgb_t> pxl=*(planarPtr1+5);
  rgb8_pixel_t pv2=pxl;
  rgb8_pixel_t pv3=*(planarPtr1+5);
     rgb8_pixel_t pv=planarPtr1[5];

    assert(*(planarPtr1+5)==planarPtr1[5]);

    rgb8_planar_ref_t planarRef=memunit_advanced_ref(planarPtr1,10);

    rgb8_step_ptr_t stepIt(&rgb8,5);
    stepIt++;
    rgb8_step_ptr_t stepIt2=stepIt+10;
    stepIt2=stepIt;

    rgb8_step_ptr_t stepIt3(&rgb8,5);

    rgb8_pixel_t& ref1=stepIt3[5];
//  bool v=boost::is_POD<iterator_traits<memory_based_step_iterator<rgb8_ptr_t> >::value_type>::value;
//  v=boost::is_POD<rgb8_pixel_t>::value;
//  v=boost::is_POD<int>::value;

    rgb8_step_ptr_t rgb8StepIt(ptr1, 10);
    rgb8_step_ptr_t rgb8StepIt2=rgb8StepIt;
    rgb8StepIt=rgb8StepIt2;
    ++rgb8StepIt;
    rgb8_ref_t reff=*rgb8StepIt; ignore_unused_variable_warning(reff);
    rgb8StepIt+=10;
    std::ptrdiff_t dst=rgb8StepIt2-rgb8StepIt; ignore_unused_variable_warning(dst);

    rgb8_pixel_t val1=ref1;
    rgb8_ptr_t ptr=&ref1;

    invert_pixel1(*planarPtr1);
//    invert_pixel1(*ptr);
    rgb8c_planar_ptr_t r8cpp;
//    invert_pixel1(*r8cpp);

    rgb8_pixel_t& val21=stepIt3[5];
    rgb8_pixel_t val22=val21;

    rgb8_pixel_t val2=stepIt3[5];
    rgb8_ptr_t ptr11=&(stepIt3[5]); ignore_unused_variable_warning(ptr11);
    rgb8_ptr_t ptr3=&*(stepIt3+5); ignore_unused_variable_warning(ptr3);

    rgb8_step_ptr_t stepIt4(ptr,5);
    ++stepIt4;

    rgb8_step_ptr_t stepIt5;
    if (stepIt4==stepIt5) {
        int st=0;ignore_unused_variable_warning(st);
    }

    iterator_from_2d<rgb8_loc_t> pix_img_it(rgb8_loc_t(ptr, 20), 5);
    ++pix_img_it;
    pix_img_it+=10;
    rgb8_pixel_t& refr=*pix_img_it;
    refr=rgb8_pixel_t(1,2,3);
    *pix_img_it=rgb8_pixel_t(1,2,3);
    pix_img_it[3]=rgb8_pixel_t(1,2,3);
    *(pix_img_it+3)=rgb8_pixel_t(1,2,3);

    iterator_from_2d<rgb8c_loc_t> pix_img_it_c(rgb8c_loc_t(rgb8c_ptr_t(ptr),20), 5);
    ++pix_img_it_c;
    pix_img_it_c+=10;
    //  *pix_img_it_c=rgb8_pixel_t(1,2,3);        // error: assigning though const iterator
    typedef iterator_from_2d<rgb8_loc_t>::difference_type dif_t;
    dif_t dt=0;
    std::ptrdiff_t tdt=dt; ignore_unused_variable_warning(tdt);

    //  memory_based_step_iterator<rgb8_pixel_t> stepIt3Err=stepIt+10;       // error: non-const from const iterator

    memory_based_2d_locator<rgb8_step_ptr_t> xy_locator(ptr,27);

    xy_locator.x()++;
//  memory_based_step_iterator<rgb8_pixel_t>& yit=xy_locator.y();
    xy_locator.y()++;
    xy_locator+=point<std::ptrdiff_t>(3,4);
    // *xy_locator=(xy_locator(-1,0)+xy_locator(0,1))/2;
    rgb8_pixel_t& rf=*xy_locator; ignore_unused_variable_warning(rf);

    make_step_iterator(rgb8_ptr_t(),3);
    make_step_iterator(rgb8_planar_ptr_t(),3);
    make_step_iterator(rgb8_planar_step_ptr_t(),3);

    // Test operator-> on planar ptrs
    {
    rgb8c_planar_ptr_t cp(&rgb8);
    rgb8_planar_ptr_t p(&rgb8);
//    get_color(p,red_t()) = get_color(cp,green_t());           // does not compile - cannot assign a non-const pointer to a const pointer. Otherwise you will be able to modify the value through it.
    }
//  xy_locator.y()++;

    // dimensions to explore
    //
    // values, references, pointers
    // color spaces (rgb,cmyk,gray)
    // channel ordering (bgr vs rgb)
    // planar vs interleaved

// Pixel POINTERS
//  typedef const iterator_traits<rgb8_ptr_t>::pointer  RGB8ConstPtr;
    typedef const rgb8_ptr_t  RGB8ConstPtr;
    typedef const rgb8_planar_ptr_t  RGB8ConstPlanarPtr;
//  typedef const iterator_traits<rgb8_planar_ptr_t>::pointer RGB8ConstPlanarPtr;

// constructing from values, references and other pointers
    RGB8ConstPtr rgb8_const_ptr=NULL; ignore_unused_variable_warning(rgb8_const_ptr);
    rgb8_ptr_t rgb8ptr=&rgb8;

    rgb8=bgr8_pixel_t(30,20,10);
    rgb8_planar_ptr_t rgb8_pptr=&rgb8;
    ++rgb8_pptr;
    rgb8_pptr--;
    rgb8_pptr[0]=rgb8;
    RGB8ConstPlanarPtr rgb8_const_planar_ptr=&rgb8;

    rgb8c_planar_ptr_t r8c=&rgb8;
    r8c=&rgb8;

    rgb8_pptr=&rgb8;

    //  rgb8_const_planar_ptr=&rgb16p;                  // error: incompatible bit depth

    //  iterator_traits<CMYK8>::pointer cmyk8_ptr_t=&rgb8;    // error: incompatible pointer type

    RGB8ConstPtr rgb8_const_ptr_err=rgb8ptr;        // const pointer from non-regular pointer
ignore_unused_variable_warning(rgb8_const_ptr_err);
// dereferencing pointers to obtain references
    rgb8_ref_t rgb8ref_2=*rgb8ptr; ignore_unused_variable_warning(rgb8ref_2);
    assert(rgb8ref_2==rgb8);
    //  RGB8Ref rgb8ref_2_err=*rgb8_const_planar_ptr;   // error: non-const reference from const pointer

    rgb8_planar_ref_t rgb8planarref_3=*rgb8_pptr; // planar reference from planar pointer
    assert(rgb8planarref_3==rgb8);
    //  RGB8Ref rgb8ref_3=*rgb8_planar_ptr_t; // error: non-planar reference from planar pointer

    const rgb8_pixel_t crgb8=rgb8;
    *rgb8_pptr=rgb8;
    *rgb8_pptr=crgb8;

    memunit_advance(rgb8_pptr,3);
    memunit_advance(rgb8_pptr,-3);
}
*/

int main()
{
    try
    {
        test_pixel_iterator();

        return EXIT_SUCCESS;
    }
    catch (std::exception const& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
}
