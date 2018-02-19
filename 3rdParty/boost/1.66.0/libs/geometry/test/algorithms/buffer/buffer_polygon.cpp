// Boost.Geometry (aka GGL, Generic Geometry Library)
// Unit Test

// Copyright (c) 2012-2014 Barend Gehrels, Amsterdam, the Netherlands.

// This file was modified by Oracle on 2016.
// Modifications copyright (c) 2016, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <test_buffer.hpp>


static std::string const simplex
    = "POLYGON ((0 0,1 5,6 1,0 0))";
static std::string const concave_simplex
    = "POLYGON ((0 0,3 5,3 3,5 3,0 0))";
static std::string const concave_b
    = "POLYGON((0 10,5 15,6 13,8 11,0 10))";
static std::string const square_simplex
    = "POLYGON ((0 0,0 1,1 1,1 0,0 0))";
static std::string const spike_simplex
    = "POLYGON ((0 0,1 5,3 3,5 5,3 3,5 1,0 0))";
static std::string const chained_box
    = "POLYGON((0 0,0 4,4 4,8 4,12 4,12 0,8 0,4 0,0 0))";

static std::string const join_types
    = "POLYGON ((0 0,0 4,4 4,2 6,0 8,2 6,4 8,8 4,4 0,0 0))"; // first 4 join types are all different: convex, concave, continue, spike

static std::string const donut_simplex
    = "POLYGON ((0 0,1 9,8 1,0 0),(1 1,4 1,1 4,1 1))";
static std::string const donut_diamond
    = "POLYGON((15 0,15 15,30 15,30 0,15 0),(26 11,22 14,19 10,23 07,26 11))";
static std::string const letter_L
    = "POLYGON ((0 0,0 4,1 4,1 1,3 1,3 0,0 0))";
static std::string const indentation
    = "POLYGON ((0 0,0 5,4 5,4 4,3 3,2 4,2 1,3 2,4 1,4 0,0 0))";
static std::string const funnelgate
    = "POLYGON((0 0,0 7,7 7,7 0,5 0,5 1,6 6,1 6,2 1,2 0,0 0))";
static std::string const gammagate
    = "POLYGON((0 0,0 6,9 6,9 0,4 0,4 2,7 2,7 4,2 4,2 0,0 0))";
static std::string const fork_a
    = "POLYGON((0 0,0 6,9 6,9 0,4 0,4 2,7 2,7 4,6 4,6 5,5 5,5 4,4 4,4 5,3 5,3 4,2 4,2 0,0 0))";
static std::string const fork_b
    = "POLYGON((0 0,0 8,14 8,14 0,4 0,4 2,13 2,13 4,12 4,12 7,9 7,9 4,7 4,7 7,4 7,4 4,2 4,2 0,0 0))";
static std::string const fork_c
    = "POLYGON((0 0,0 9,12 9,12 0,4 0,4 4,6 4,6 2,8 2,8 4,10 4,10 7,6 7,6 6,2 6,2 0,0 0))";

static std::string const arrow
    = "POLYGON ((1 0,1 5,0.5 4.5,2 10,3.5 4.5,3 5,3 0,1 0))";
static std::string const tipped_aitch
    = "POLYGON ((0 0,0 3,3 3,3 4,0 4,0 7,7 7,7 4,4 4,4 3,7 3,7 0,0 0))";
static std::string const snake
    = "POLYGON ((0 0,0 3,3 3,3 4,0 4,0 7,8 7,8 4,6 4,6 3,8 3,8 0,7 0,7 2,5 2"
                ",5 5,7 5,7 6,1 6,1 5,4 5,4 2,1 2,1 1,6 1,6 0,0 0))";
static std::string const church
    = "POLYGON ((0 0,0 3,2.999 3,3 8,3 0,0 0))";
static std::string const flower
    = "POLYGON ((1 0,1 10,9 10,9 0,4.99 0,4.99 5.5,4.5 6,5 6.5,5.5 6,5.01 5.5,5.01 0.01,5.25 0.01,5.25 5,6 3,8 5,6 6,8 7,6 9,5 7,4 9,2 7,4 6,2 5,4 3,4.75 5,4.75 0,1 0))";

static std::string const saw
    = "POLYGON((1 3,1 8,1.5 6,5 8,5.5 6,9 8,9.5 6,13 8,13 3,1 3))";

static std::string const bowl
    = "POLYGON((1 2,1 7,2 7,3 5,5 4,7 5,8 7,9 7,9 2,1 2))";

// Triangle with segmented sides, closing point at longest side
static std::string const triangle
    = "POLYGON((4 5,5 4,4 4,3 4,3 5,3 6,4 5))";

// Triangle which caused acos in join_round to fail with larger side strategy
static std::string const sharp_triangle
    = "POLYGON((2 0,3 8,4 0,2 0))";

static std::string const right_triangle
    = "POLYGON((0 1,20 0,0 0,0 1))";


static std::string const degenerate0
    = "POLYGON(())";
static std::string const degenerate1
    = "POLYGON((5 5))";
static std::string const degenerate2
    = "POLYGON((5 5,5 5,5 5,5 5))";
static std::string const degenerate3
    = "POLYGON((0 0,0 10,10 10,10 0,0 0),(5 5,5 5,5 5,5 5))";


// Real-life examples
static std::string const county1
    = "POLYGON((-111.700 41.200 ,-111.681388 41.181739 ,-111.682453 41.181506 ,-111.684052 41.180804 ,-111.685295 41.180538 ,-111.686318 41.180776 ,-111.687517 41.181416 ,-111.688982 41.181520 ,-111.690670 41.181523 ,-111.692135 41.181460 ,-111.693646 41.182034 ,-111.695156 41.182204 ,-111.696489 41.182274 ,-111.697775 41.182075 ,-111.698974 41.181539 ,-111.700485 41.182348 ,-111.701374 41.182955 ,-111.700 41.200))";

//static std::string const invalid_parcel
//    =  "POLYGON((116042.20 464335.07,116056.35 464294.59,116066.41 464264.16,116066.44 464264.09,116060.35 464280.93,116028.89 464268.43,116028.89 464268.44,116024.74 464280.7,116018.91 464296.71,116042.2 464335.07))";

static std::string const parcel1
    = "POLYGON((225343.489 585110.376,225319.123 585165.731,225323.497 585167.287,225323.134 585167.157,225313.975 585169.208,225321.828 585172,225332.677 585175.83,225367.032 585186.977,225401.64 585196.671,225422.799 585201.029,225429.784 585202.454,225418.859 585195.112,225423.803 585196.13,225425.389 585196.454,225397.027 585165.48,225363.802 585130.372,225354.086 585120.261,225343.489 585110.376))";
static std::string const parcel2
    = "POLYGON((173356.986490154 605912.122380707,173358.457939143 605902.891897507,173358.458257372 605902.889901239,173214.162964795 605901.13020255,173214.162746654 605901.132200038,173213.665 605905.69,173212.712441616 605913.799985923,173356.986490154 605912.122380707))";
static std::string const parcel3
    = "POLYGON((120528.56 462115.62,120533.4 462072.1,120533.4 462072.01,120533.39 462071.93,120533.36 462071.86,120533.33 462071.78,120533.28 462071.72,120533.22 462071.66,120533.15 462071.61,120533.08 462071.58,120533 462071.55,120532.92 462071.54,120467.68 462068.66,120468.55 462059.04,120517.39 462062.87,120517.47 462062.87,120517.55 462062.86,120517.62 462062.83,120517.69 462062.79,120517.76 462062.74,120517.81 462062.68,120517.86 462062.62,120517.89 462062.55,120517.92 462062.47,120530.49 461998.63,120530.5 461998.55,120530.49 461998.47,120530.47 461998.39,120530.44 461998.31,120530.4 461998.24,120530.35 461998.18,120530.28 461998.13,120530.21 461998.09,120530.13 461998.06,120482.19 461984.63,120485 461963.14,120528.2 461950.66,120528.28 461950.63,120528.35 461950.59,120528.42 461950.53,120528.47 461950.47,120528.51 461950.4,120528.54 461950.32,120528.56 461950.24,120528.56 461950.15,120528.55 461950.07,120528.53 461949.99,120528.49 461949.92,120528.44 461949.85,120497.49 461915.03,120497.43 461914.98,120497.37 461914.93,120497.3 461914.9,120497.23 461914.88,120497.15 461914.86,120424.61 461910.03,120424.53 461910.03,120424.45 461910.05,120424.37 461910.07,120424.3 461910.11,120424.24 461910.16,120424.18 461910.22,120424.14 461910.29,120424.11 461910.37,120424.09 461910.45,120424.08 461910.53,120424.08 461967.59,120424.08 461967.67,120424.1 461967.75,120424.14 461967.82,120424.18 461967.89,120424.23 461967.95,120424.3 461968,120424.37 461968.04,120424.44 461968.07,120424.52 461968.09,120473.31 461973.83,120469.63 461993.16,120399.48 461986.43,120399.4 461986.43,120399.32 461986.44,120399.25 461986.47,120399.17 461986.5,120399.11 461986.55,120399.05 461986.61,120399.01 461986.67,120398.97 461986.74,120398.95 461986.82,120398.93 461986.9,120394.1 462057.5,120394.1 462057.58,120394.11 462057.66,120394.14 462057.74,120394.18 462057.81,120394.23 462057.87,120394.29 462057.93,120394.35 462057.97,120394.43 462058,120394.5 462058.03,120394.58 462058.03,120458.74 462059.95,120455.16 462072.48,120396.57 462067.68,120396.49 462067.68,120396.4 462067.69,120396.32 462067.72,120396.25 462067.76,120396.18 462067.82,120396.13 462067.88,120396.08 462067.96,120396.05 462068.04,120396.03 462068.12,120392.17 462103.9,120392.16 462103.99,120392.18 462104.07,120392.2 462104.15,120392.24 462104.22,120392.29 462104.29,120392.35 462104.35,120392.42 462104.4,120392.5 462104.43,120392.58 462104.45,120392.66 462104.46,120393.63 462104.46,120393.63 462103.46,120393.22 462103.46,120396.98 462068.71,120455.49 462073.51,120455.57 462073.51,120455.66 462073.49,120455.74 462073.46,120455.81 462073.42,120455.88 462073.37,120455.93 462073.3,120455.98 462073.23,120456.01 462073.15,120459.88 462059.61,120459.89 462059.52,120459.9 462059.44,120459.88 462059.36,120459.86 462059.28,120459.82 462059.21,120459.77 462059.14,120459.72 462059.08,120459.65 462059.04,120459.57 462059,120459.49 462058.98,120459.41 462058.97,120395.13 462057.05,120399.9 461987.48,120469.99 461994.2,120470.07 461994.2,120470.15 461994.19,120470.23 461994.16,120470.3 461994.13,120470.37 461994.08,120470.42 461994.02,120470.47 461993.95,120470.5 461993.88,120470.53 461993.8,120474.4 461973.48,120474.4 461973.4,120474.4 461973.32,120474.38 461973.24,120474.35 461973.16,120474.31 461973.09,120474.25 461973.03,120474.19 461972.98,120474.12 461972.94,120474.04 461972.91,120473.96 461972.9,120425.08 461967.14,120425.08 461911.06,120496.88 461915.85,120527.16 461949.92,120484.4 461962.27,120484.33 461962.3,120484.25 461962.35,120484.19 461962.4,120484.14 461962.46,120484.09 461962.53,120484.06 461962.61,120484.05 461962.69,120481.14 461984.93,120481.14 461985.01,120481.15 461985.09,120481.17 461985.17,120481.2 461985.24,120481.25 461985.31,120481.3 461985.36,120481.36 461985.41,120481.43 461985.45,120481.51 461985.48,120529.42 461998.9,120517.02 462061.84,120468.14 462058,120468.05 462058,120467.97 462058.02,120467.89 462058.05,120467.81 462058.09,120467.75 462058.15,120467.69 462058.22,120467.65 462058.29,120467.62 462058.37,120467.6 462058.46,120466.64 462069.1,120466.63 462069.18,120466.65 462069.26,120466.67 462069.33,120466.71 462069.4,120466.76 462069.47,120466.81 462069.53,120466.88 462069.57,120466.95 462069.61,120467.03 462069.63,120467.11 462069.64,120532.34 462072.52,120527.62 462115.03,120391.73 462106.36,120391.66 462107.36,120528.03 462116.06,120528.12 462116.06,120528.2 462116.04,120528.28 462116.02,120528.35 462115.97,120528.42 462115.92,120528.47 462115.85,120528.51 462115.78,120528.54 462115.7,120528.56 462115.62))";

static std::string const parcel3_bend // of parcel_3 - clipped
    = "POLYGON((120399.40000152588 461986.43000030518, 120399.47999954224 461986.43000030518, 120403 461986.76769953477, 120403 461987.777217312, 120399.90000152588 461987.47999954224, 120399.72722010587 461990, 120398.71791817161 461990, 120398.93000030518 461986.90000152588, 120398.95000076294 461986.81999969482, 120398.9700012207 461986.74000167847, 120399.00999832153 461986.66999816895, 120399.04999923706 461986.61000061035, 120399.11000061035 461986.54999923706, 120399.16999816895 461986.5, 120399.25 461986.4700012207, 120399.31999969482 461986.43999862671, 120399.40000152588 461986.43000030518))";

// Has concavety
static std::string const italy_part1
    = "POLYGON ((1660000 5190000 , 1651702.9375 5167014.5, 1650311.34375 5167763.53125, 1643318.59375 5172188.15625, 1642488.03125 5172636.75, 1640818.25 5173802.75, 1640107.03125 5174511.21875, 1638931.9375 5176054.03125, 1638684.5625 5177095.75, 1660000 5190000))";

// Did have a self-intersection for first flat/convex solution
static std::string const nl_part1
    = "POLYGON ((471871.966884626832325 6683515.683521211147308,470695.876464393688366 6681756.129975977353752,469211.542374156008009 6679924.978601523675025,464542.357652322971262 6674631.078279769048095,463243.59315323777264 6673494.345109832473099,459502.033748187706806 6670774.304660517722368,452204.484529234410729 6666030.372161027044058,439990.287360413349234 6659313.515823394991457,434547.988775020290632 6657783.034025833010674,433867.715366783668287 6657685.311832630075514,433063.65468478546245 6657783.034025833010674,423725.285241116478574 6659758.338574352674186,422581.143514745577704 6660350.880751021206379,422333.791606202081311 6660844.461689073592424,421993.599242338153999 6662622.453031159006059,421993.599242338153999 6663363.130126810632646,422612.090333183819894 6667314.244697011075914,422241.062470370030496 6667759.324870104901493,421096.80942450783914 6668303.522556710988283,408449.802075482031796 6673245.655735924839973,401646.845354124438018 6675273.665065255947411,400842.895991614612285 6675372.844085373915732,400255.351719210040756 6675224.699206517077982,392370.258227849320974 6672455.311684883199632,391968.283546594379004 6672009.975794731639326,391875.554410762328189 6671219.573235900141299,391937.336728153284639 6670527.121663697995245,392122.906319305824582 6669933.841785561293364,392339.311409408226609 6667957.501854715868831,392308.475910458364524 6665733.178529324010015,391937.336728153284639 6665289.631341196596622,387793.802641845482867 6664449.731981083750725,385814.7647345236619 6664202.740090588107705,384268.648326894966885 6664300.5401631873101,382846.319193030707538 6664646.406163938343525,382289.610419573145919 6664943.560305980034173,377186.502322628628463 6668957.888622742146254,376352.608017096004914 6669834.728738470934331,376197.985244384559337 6670428.001423852518201,375548.658654586179182 6676313.056885857135057,375243.086652359867003 6687692.866469252854586,379228.324422756850254 6689778.692146780900657,391782.713955441897269 6694388.125634397380054,393885.427817036863416 6694487.537080916576087,395215.139134563156404 6694091.147844122722745,405171.999669074953999 6689084.665672835893929,414263.128523688821588 6684478.40333267301321,415778.298112876422238 6683439.398042757064104,416396.677884233708028 6683192.009851422160864,419025.042381353967357 6682597.809754087589681,429909.63955213711597 6681508.792763692326844,430497.183824544539675 6681656.873437026515603,440979.806314075656701 6686955.330480101518333,467325.344922157470137 6687797.546342735178769,468129.294284664443694 6687698.216345063410699,468747.785375512961764 6687450.699095219373703,469211.542374156008009 6687054.651439971290529,469489.952420631831046 6686707.835750493220985,471871.966884626832325 6683515.683521211147308))";

static std::string const erode_triangle ="POLYGON((-262.6446228027343700 -261.9999389648437500, -261.0000000000000000 -262.0000000000000000, -261.0000915527343700 -262.5285644531250000, -262.6446228027343700 -261.9999389648437500))";

static std::string const forming_uu_a ="POLYGON((0 0,0 10,5 6,10 10,10 0,5 4,0 0))";
static std::string const forming_uu_b ="POLYGON((0 0,0 10,5 6,10 10,10 0,5 4,0 0),(1.25 2.5, 4.25 5,1.25 7.5,1.25 5.5,2.25 5.0,1.25 4.5,1.25 2.5))";

// Ticket 10398, fails at next distances ( /10.0 ):
// #1: 5,25,84
// #2: 5,13,45,49,60,62,66,73
// #3: 4,8,12,35,45,54
// #4: 6,19,21,23,30,43,45,66,78,91

static std::string const ticket_10398_1
    = "POLYGON((897866.5 6272518.7,897882.5 6272519.2,897882.6 6272519,897883.3 6272508.7,897883.5 6272505.5,897855 6272503.5,897852.4 6272505.6,897850.1 6272517.6,897860.8 6272518.5,897866.5 6272518.7))";
static std::string const ticket_10398_2
    = "POLYGON((898882.3 6271337.3,898895.7 6271339.9,898898 6271328.3,898881.6 6271325.1,898879.3 6271336.7,898882.3 6271337.3))";
static std::string const ticket_10398_3
    = "POLYGON((897558.7 6272055,897552.5 6272054.2,897552.5 6272053.7,897546.1 6272052.7,897545.6 6272057.7,897560.7 6272059.6,897560.9 6272055.3,897558.7 6272055))";
static std::string const ticket_10398_4
    = "POLYGON((898563.3 6272366.9,898554.7 6272379.2,898559.7 6272382.3,898561.6 6272379.4,898568.7 6272369.1,898563.8 6272366.2,898563.3 6272366.9))";

static std::string const ticket_10412
    = "POLYGON((897747.8 6270564.3,897764.3 6270569.7,897776.5 6270529.5,897768.1 6270527.1,897767.6 6270529.4,897756.3 6270525.8,897745.8 6270522.3,897752 6270502.9,897749.7 6270502,897750.7 6270499.1,897751.8 6270498.6,897752.3 6270499.3,897754.6 6270497.9,897755.8 6270500.2,897766.8 6270494.1,897765.6 6270491.5,897768.3 6270490.5,897770.9 6270491.5,897770.2 6270494.6,897780.1 6270497.5,897781 6270494.6,897786.8 6270496.6,897790.8 6270482.5,897785.3 6270480.7,897785.9 6270478.2,897768.9 6270473.2,897768.1 6270475.8,897766.1 6270475.2,897758.7 6270479.2,897753.2 6270481.8,897751.9 6270479,897746.5 6270481.9,897748 6270484.6,897745.2 6270486.1,897743.9 6270483.3,897741.4 6270484.7,897742.6 6270487.3,897739.4 6270488.9,897738.3 6270486.3,897735.6 6270487.8,897733.1 6270496.8,897731.2 6270502.7,897732.4 6270503.2,897731.5 6270506.1,897730.3 6270505.7,897725.8 6270520.2,897726.8 6270520.7,897726 6270523,897728 6270523.7,897726.3 6270529.6,897742.8 6270534.5,897741.2 6270539.9,897751.4 6270543.4,897750.7 6270546.4,897753.2 6270547.2,897747.8 6270564.3))";

static std::string const ticket_11580
    = "POLYGON((14.02 474.96,14.02 494.96,14.022 494.96,14.022 486.24,14.02 474.96))";

// CCW Polygons not working in 1.56
static std::string const mysql_report_2014_10_24
    = "POLYGON((0 0, 0 8, 8 8, 8 10, -10 10, -10 0, 0 0))";
static std::string const mysql_report_2014_10_28_1
    = "POLYGON((0 0,10 10,0 8,0 0))";
static std::string const mysql_report_2014_10_28_2
    = "POLYGON((1 1,10 10,0 8,1 1))";
static std::string const mysql_report_2014_10_28_3
    = "POLYGON((2 2,8 2,8 8,2 8,2 2))";

// Polygons having problems with negative distances in 1.57
static std::string const mysql_report_2015_02_17_1
    = "POLYGON((0 0,0 10,10 10,10 0,0 0),(4 4,4 6,6 6,6 4,4 4))";
static std::string const mysql_report_2015_02_17_2
    = "POLYGON((0 0,0 10,10 10,10 0,0 0))";
static std::string const mysql_report_2015_02_17_3
    = "POLYGON((10 10,10 20,20 20,20 10,10 10))";

// Polygons causing assertion failure
static std::string const mysql_report_2015_07_05_0
    = "POLYGON((0 0,0 30000,30000 20000,25000 0,0 0),(-11 3,4.99294e+306 1.78433e+307,15 -14,-11 3))";
static std::string const mysql_report_2015_07_05_1
    = "POLYGON((9192 27876,128 4.5036e+15,-1 5,3 9,9192 27876),(-11 3,4.99294e+306 1.78433e+307,15 -14,-11 3))";
static std::string const mysql_report_2015_07_05_2
    = "POLYGON((-15 -15,1.31128e+308 2.47964e+307,-20 -9,-15 -15),(11 -4,1.04858e+06 5.36871e+08,7.03687e+13 -1.15292e+18,-12 17,10284 -21812,11 -4),(-28066 -10001,2.19902e+12 3.35544e+07,8.80784e+306 1.04773e+308,1.42177e+308 1.6141e+308,-28066 -10001))";
static std::string const mysql_report_2015_07_05_3
    = "POLYGON((-2.68435e+08 -12,27090 -14130,5.76461e+17 5.49756e+11,-2.68435e+08 -12),(2.68435e+08 65539,1.42845e+308 1.63164e+308,-1 -8,-4 10,2.68435e+08 65539),(13 17,8.79609e+12 -2.2518e+15,1.02101e+308 3.13248e+306,17868 780,5 -4,13 17),(-1 14,1.35905e+308 2.09331e+307,1.37439e+11 -2047,-1 14))";
static std::string const mysql_report_2015_07_05_4
    = "POLYGON((2.19902e+12 524287,1.13649e+308 1.36464e+308,-10 -19,2.19902e+12 524287),(1.44115e+17 -1.09951e+12,-14 0,-4347 16243,1.44115e+17 -1.09951e+12))";
static std::string const mysql_report_2015_07_05_5
    = "POLYGON((9 3,-8193 8.38861e+06,-2 -4,9 3),(-10 -2,32268 -2557,1.72036e+308 5.67867e+307,4.91634e+307 1.41031e+308,-2.68435e+08 -19,-10 -2),(-5 4,9.50167e+307 1.05883e+308,-422 -25737,-5 4))";

// Versions without interiors
static std::string const mysql_report_2015_07_05_0_wi
    = "POLYGON((0 0,0 30000,30000 20000,25000 0,0 0))";
static std::string const mysql_report_2015_07_05_1_wi
    = "POLYGON((9192 27876,128 4.5036e+15,-1 5,3 9,9192 27876))";
static std::string const mysql_report_2015_07_05_2_wi
    = "POLYGON((-15 -15,1.31128e+308 2.47964e+307,-20 -9,-15 -15))";
static std::string const mysql_report_2015_07_05_3_wi
    = "POLYGON((-2.68435e+08 -12,27090 -14130,5.76461e+17 5.49756e+11,-2.68435e+08 -12))";
static std::string const mysql_report_2015_07_05_4_wi
    = "POLYGON((2.19902e+12 524287,1.13649e+308 1.36464e+308,-10 -19,2.19902e+12 524287))";
static std::string const mysql_report_2015_07_05_5_wi
    = "POLYGON((9 3,-8193 8.38861e+06,-2 -4,9 3))";

class buffer_custom_side_strategy
{
public :
    template
    <
        typename Point,
        typename OutputRange,
        typename DistanceStrategy
    >
    static inline bg::strategy::buffer::result_code apply(
                Point const& input_p1, Point const& input_p2,
                bg::strategy::buffer::buffer_side_selector side,
                DistanceStrategy const& distance,
                OutputRange& output_range)
    {
        // Generate a block along (left or right of) the segment

        double const dx = bg::get<0>(input_p2) - bg::get<0>(input_p1);
        double const dy = bg::get<1>(input_p2) - bg::get<1>(input_p1);

        // For normalization [0,1] (=dot product d.d, sqrt)
        double const length = bg::math::sqrt(dx * dx + dy * dy);

        if (bg::math::equals(length, 0))
        {
            return bg::strategy::buffer::result_no_output;
        }

        // Generate the perpendicular p, to the left (ccw), and use an adapted distance
        double const d = 1.1 * distance.apply(input_p1, input_p2, side);
        double const px = d * -dy / length;
        double const py = d * dx / length;

        output_range.resize(2);

        bg::set<0>(output_range.front(), bg::get<0>(input_p1) + px);
        bg::set<1>(output_range.front(), bg::get<1>(input_p1) + py);
        bg::set<0>(output_range.back(), bg::get<0>(input_p2) + px);
        bg::set<1>(output_range.back(), bg::get<1>(input_p2) + py);

        return bg::strategy::buffer::result_normal;
    }
};


template <bool Clockwise, typename P>
void test_all()
{
    typedef bg::model::polygon<P, Clockwise, true> polygon_type;

    bg::strategy::buffer::join_miter join_miter(10.0);
    bg::strategy::buffer::join_round join_round(100);
    bg::strategy::buffer::join_round join_round_rough(12);
    bg::strategy::buffer::end_flat end_flat;
    bg::strategy::buffer::end_round end_round(100);

    test_one<polygon_type, polygon_type>("simplex", simplex, join_round, end_flat, 47.9408, 1.5);
    test_one<polygon_type, polygon_type>("simplex", simplex, join_miter, end_flat, 52.8733, 1.5);

    test_one<polygon_type, polygon_type>("simplex", simplex, join_round, end_flat, 7.04043, -0.5);
    test_one<polygon_type, polygon_type>("simplex", simplex, join_miter, end_flat, 7.04043, -0.5);

    test_one<polygon_type, polygon_type>("square_simplex", square_simplex, join_round, end_flat, 14.0639, 1.5);
    test_one<polygon_type, polygon_type>("square_simplex", square_simplex, join_miter, end_flat, 16.0000, 1.5);

    test_one<polygon_type, polygon_type>("square_simplex01", square_simplex, join_miter, end_flat, 0.6400, -0.1);
    test_one<polygon_type, polygon_type>("square_simplex04", square_simplex, join_miter, end_flat, 0.0400, -0.4);
    test_one<polygon_type, polygon_type>("square_simplex05", square_simplex, join_miter, end_flat, 0.0, -0.5);
    test_one<polygon_type, polygon_type>("square_simplex06", square_simplex, join_miter, end_flat, 0.0, -0.6);

    test_one<polygon_type, polygon_type>("concave_simplex", concave_simplex, join_round, end_flat, 14.5616, 0.5);
    test_one<polygon_type, polygon_type>("concave_simplex", concave_simplex, join_miter, end_flat, 16.3861, 0.5);

    test_one<polygon_type, polygon_type>("concave_simplex", concave_simplex, join_round, end_flat, 0.777987, -0.5);
    test_one<polygon_type, polygon_type>("concave_simplex", concave_simplex, join_miter, end_flat, 0.724208, -0.5);

    test_one<polygon_type, polygon_type>("concave_b10", concave_b, join_miter, end_flat, 836.9106, 10.0);
    test_one<polygon_type, polygon_type>("concave_b25", concave_b, join_miter, end_flat, 4386.6479, 25.0);
    test_one<polygon_type, polygon_type>("concave_b50", concave_b, join_miter, end_flat, 16487.2000, 50.0);
    test_one<polygon_type, polygon_type>("concave_b75", concave_b, join_miter, end_flat, 36318.1506, 75.0);
    test_one<polygon_type, polygon_type>("concave_b100", concave_b, join_miter, end_flat, 63879.5186, 100.0);

    test_one<polygon_type, polygon_type>("concave_b10", concave_b, join_round, end_flat, 532.2875, 10.0);
    test_one<polygon_type, polygon_type>("concave_b25", concave_b, join_round, end_flat, 2482.8329, 25.0);
    test_one<polygon_type, polygon_type>("concave_b50", concave_b, join_round, end_flat, 8872.9719, 50.0);
    test_one<polygon_type, polygon_type>("concave_b75", concave_b, join_round, end_flat, 19187.5490, 75.0);
    test_one<polygon_type, polygon_type>("concave_b100", concave_b, join_round, end_flat, 33426.6139, 100.0);

    test_one<polygon_type, polygon_type>("concave_b_rough_10", concave_b, join_round_rough, end_flat, 520.312, 10.0);
    test_one<polygon_type, polygon_type>("concave_b_rough_25", concave_b, join_round_rough, end_flat, 2409.384, 25.0);
    test_one<polygon_type, polygon_type>("concave_b_rough_50", concave_b, join_round_rough, end_flat, 8586.812, 50.0);
    test_one<polygon_type, polygon_type>("concave_b_rough_75", concave_b, join_round_rough, end_flat, 18549.018, 75.0);
    test_one<polygon_type, polygon_type>("concave_b_rough_100", concave_b, join_round_rough, end_flat, 32295.917, 100.0);

    test_one<polygon_type, polygon_type>("spike_simplex15", spike_simplex, join_round, end_round, 50.3633, 1.5);
    test_one<polygon_type, polygon_type>("spike_simplex15", spike_simplex, join_miter, end_flat, 51.5509, 1.5);
    test_one<polygon_type, polygon_type>("spike_simplex30", spike_simplex, join_round, end_round, 100.9199, 3.0);
    test_one<polygon_type, polygon_type>("spike_simplex30", spike_simplex, join_miter, end_flat, 106.6979, 3.0);
    test_one<polygon_type, polygon_type>("spike_simplex150", spike_simplex, join_round, end_round, 998.9821, 15.0);
    test_one<polygon_type, polygon_type>("spike_simplex150", spike_simplex, join_miter, end_flat, 1428.1560, 15.0);

    test_one<polygon_type, polygon_type>("join_types", join_types, join_round, end_flat, 88.2060, 1.5);

    test_one<polygon_type, polygon_type>("chained_box", chained_box, join_round, end_flat, 83.1403, 1.0);
    test_one<polygon_type, polygon_type>("chained_box", chained_box, join_miter, end_flat, 84, 1.0);
    test_one<polygon_type, polygon_type>("L", letter_L, join_round, end_flat, 13.7314, 0.5);
    test_one<polygon_type, polygon_type>("L", letter_L, join_miter, end_flat, 14.0, 0.5);

    test_one<polygon_type, polygon_type>("chained_box", chained_box, join_miter, end_flat, 84, 1.0);
    test_one<polygon_type, polygon_type>("chained_box", chained_box, join_round, end_flat, 83.1403, 1.0);

    test_one<polygon_type, polygon_type>("indentation4", indentation, join_miter, end_flat, 25.7741, 0.4);
    test_one<polygon_type, polygon_type>("indentation4", indentation, join_round, end_flat, 25.5695, 0.4);
    test_one<polygon_type, polygon_type>("indentation5", indentation, join_miter, end_flat, 28.2426, 0.5);
    test_one<polygon_type, polygon_type>("indentation5", indentation, join_round, end_flat, 27.9953, 0.5);
    test_one<polygon_type, polygon_type>("indentation6", indentation, join_miter, end_flat, 30.6712, 0.6);

    // SQL Server gives 30.34479159164
    test_one<polygon_type, polygon_type>("indentation6", indentation, join_round, end_flat, 30.3445, 0.6);

    test_one<polygon_type, polygon_type>("indentation7", indentation, join_miter, end_flat, 33.0958, 0.7);
    test_one<polygon_type, polygon_type>("indentation7", indentation, join_round, end_flat, 32.6533, 0.7);

    test_one<polygon_type, polygon_type>("indentation8", indentation, join_miter, end_flat, 35.5943, 0.8);
    test_one<polygon_type, polygon_type>("indentation8", indentation, join_round, end_flat, 35.0164, 0.8);
    test_one<polygon_type, polygon_type>("indentation12", indentation, join_miter, end_flat, 46.3541, 1.2);
    test_one<polygon_type, polygon_type>("indentation12", indentation, join_round, end_flat, 45.0537, 1.2);

    // Indentation - deflated
    test_one<polygon_type, polygon_type>("indentation4", indentation, join_miter, end_flat, 6.991, -0.4);
    test_one<polygon_type, polygon_type>("indentation4", indentation, join_round, end_flat, 7.255, -0.4);
    test_one<polygon_type, polygon_type>("indentation8", indentation, join_miter, end_flat, 1.369, -0.8);
    test_one<polygon_type, polygon_type>("indentation8", indentation, join_round, end_flat, 1.374, -0.8);
    test_one<polygon_type, polygon_type>("indentation12", indentation, join_miter, end_flat, 0, -1.2);
    test_one<polygon_type, polygon_type>("indentation12", indentation, join_round, end_flat, 0, -1.2);

    test_one<polygon_type, polygon_type>("donut_simplex6", donut_simplex, join_miter, end_flat, 53.648, 0.6);
    test_one<polygon_type, polygon_type>("donut_simplex6", donut_simplex, join_round, end_flat, 52.820, 0.6);
    test_one<polygon_type, polygon_type>("donut_simplex8", donut_simplex, join_miter, end_flat, 61.132, 0.8);
    test_one<polygon_type, polygon_type>("donut_simplex8", donut_simplex, join_round, end_flat, 59.6713, 0.8);
    test_one<polygon_type, polygon_type>("donut_simplex10", donut_simplex, join_miter, end_flat, 68.670, 1.0);
    test_one<polygon_type, polygon_type>("donut_simplex10", donut_simplex, join_round, end_flat, 66.387, 1.0);
    test_one<polygon_type, polygon_type>("donut_simplex12", donut_simplex, join_miter, end_flat, 76.605, 1.2);
    test_one<polygon_type, polygon_type>("donut_simplex12", donut_simplex, join_round, end_flat, 73.3179, 1.2);
    test_one<polygon_type, polygon_type>("donut_simplex14", donut_simplex, join_miter, end_flat, 84.974, 1.4);
    test_one<polygon_type, polygon_type>("donut_simplex14", donut_simplex, join_round, end_flat, 80.500, 1.4);
    test_one<polygon_type, polygon_type>("donut_simplex16", donut_simplex, join_miter, end_flat, 93.777, 1.6);
    test_one<polygon_type, polygon_type>("donut_simplex16", donut_simplex, join_round, end_flat, 87.933, 1.6);

    test_one<polygon_type, polygon_type>("donut_simplex3", donut_simplex, join_miter, end_flat, 19.7636, -0.3);
    test_one<polygon_type, polygon_type>("donut_simplex3", donut_simplex, join_round, end_flat, 19.8861, -0.3);
    test_one<polygon_type, polygon_type>("donut_simplex6", donut_simplex, join_miter, end_flat, 12.8920, -0.6);
    test_one<polygon_type, polygon_type>("donut_simplex6", donut_simplex, join_round, end_flat, 12.9157, -0.6);

    test_one<polygon_type, polygon_type>("donut_diamond1", donut_diamond, join_miter, end_flat, 280.0, 1.0);
    test_one<polygon_type, polygon_type>("donut_diamond4", donut_diamond, join_miter, end_flat, 529.0, 4.0);
    test_one<polygon_type, polygon_type>("donut_diamond5", donut_diamond, join_miter, end_flat, 625.0, 5.0);
    test_one<polygon_type, polygon_type>("donut_diamond6", donut_diamond, join_miter, end_flat, 729.0, 6.0);

    test_one<polygon_type, polygon_type>("donut_diamond1", donut_diamond, join_miter, end_flat, 122.0417, -1.0);
    test_one<polygon_type, polygon_type>("donut_diamond2", donut_diamond, join_miter, end_flat, 56.3750, -2.0);
    test_one<polygon_type, polygon_type>("donut_diamond3", donut_diamond, join_miter, end_flat, 17.7084, -3.0);

    test_one<polygon_type, polygon_type>("arrow4", arrow, join_miter, end_flat, 28.265, 0.4);
    test_one<polygon_type, polygon_type>("arrow4", arrow, join_round, end_flat, 27.039, 0.4);
    test_one<polygon_type, polygon_type>("arrow5", arrow, join_miter, end_flat, 31.500, 0.5);
    test_one<polygon_type, polygon_type>("arrow5", arrow, join_round, end_flat, 29.621, 0.5);
    test_one<polygon_type, polygon_type>("arrow6", arrow, join_miter, end_flat, 34.903, 0.6);
    test_one<polygon_type, polygon_type>("arrow6", arrow, join_round, end_flat, 32.268, 0.6);

    test_one<polygon_type, polygon_type>("tipped_aitch3", tipped_aitch, join_miter, end_flat, 55.36, 0.3);
    test_one<polygon_type, polygon_type>("tipped_aitch9", tipped_aitch, join_miter, end_flat, 77.44, 0.9);
    test_one<polygon_type, polygon_type>("tipped_aitch13", tipped_aitch, join_miter, end_flat, 92.16, 1.3);

    // SQL Server: 55.205415532967 76.6468846383224 90.642916957136
    test_one<polygon_type, polygon_type>("tipped_aitch3", tipped_aitch, join_round, end_flat, 55.2053, 0.3);
    test_one<polygon_type, polygon_type>("tipped_aitch9", tipped_aitch, join_round, end_flat, 76.6457, 0.9);
    test_one<polygon_type, polygon_type>("tipped_aitch13", tipped_aitch, join_round, end_flat, 90.641, 1.3);

    test_one<polygon_type, polygon_type>("snake4", snake, join_miter, end_flat, 64.44, 0.4);
    test_one<polygon_type, polygon_type>("snake5", snake, join_miter, end_flat, 72, 0.5);
    test_one<polygon_type, polygon_type>("snake6", snake, join_miter, end_flat, 75.44, 0.6);
#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<polygon_type, polygon_type>("snake16", snake, join_miter, end_flat, 114.24, 1.6);
#endif

    test_one<polygon_type, polygon_type>("funnelgate2", funnelgate, join_miter, end_flat, 120.982, 2.0);
    test_one<polygon_type, polygon_type>("funnelgate3", funnelgate, join_miter, end_flat, 13.0*13.0, 3.0);
    test_one<polygon_type, polygon_type>("funnelgate4", funnelgate, join_miter, end_flat, 15.0*15.0, 4.0);
    test_one<polygon_type, polygon_type>("gammagate1", gammagate, join_miter, end_flat, 88.0, 1.0);
    test_one<polygon_type, polygon_type>("fork_a1", fork_a, join_miter, end_flat, 88.0, 1.0);
    test_one<polygon_type, polygon_type>("fork_b1", fork_b, join_miter, end_flat, 154.0, 1.0);
    test_one<polygon_type, polygon_type>("fork_c1", fork_c, join_miter, end_flat, 152.0, 1.0);
    test_one<polygon_type, polygon_type>("triangle", triangle, join_miter, end_flat, 14.6569, 1.0);

    test_one<polygon_type, polygon_type>("degenerate0", degenerate0, join_round, end_round, 0.0, 1.0);
    test_one<polygon_type, polygon_type>("degenerate1", degenerate1, join_round, end_round, 3.1389, 1.0);
    test_one<polygon_type, polygon_type>("degenerate2", degenerate2, join_round, end_round, 3.1389, 1.0);
    test_one<polygon_type, polygon_type>("degenerate3", degenerate3, join_round, end_round, 143.1395, 1.0);

    test_one<polygon_type, polygon_type>("gammagate2", gammagate, join_miter, end_flat, 130.0, 2.0);

    test_one<polygon_type, polygon_type>("flower1", flower, join_miter, end_flat, 67.614, 0.1);
    test_one<polygon_type, polygon_type>("flower20", flower, join_miter, end_flat, 74.894, 0.20);
    test_one<polygon_type, polygon_type>("flower25", flower, join_miter, end_flat, 78.226, 0.25);
    test_one<polygon_type, polygon_type>("flower30", flower, join_miter, end_flat, 81.492, 0.30);
    test_one<polygon_type, polygon_type>("flower35", flower, join_miter, end_flat, 84.694, 0.35);
    test_one<polygon_type, polygon_type>("flower40", flower, join_miter, end_flat, 87.831, 0.40);
    test_one<polygon_type, polygon_type>("flower45", flower, join_miter, end_flat, 90.902, 0.45);
    test_one<polygon_type, polygon_type>("flower50", flower, join_miter, end_flat, 93.908, 0.50);
    test_one<polygon_type, polygon_type>("flower55", flower, join_miter, end_flat, 96.849, 0.55);
    test_one<polygon_type, polygon_type>("flower60", flower, join_miter, end_flat, 99.724, 0.60);

    test_one<polygon_type, polygon_type>("flower10", flower, join_round, end_flat, 67.486, 0.10);
    test_one<polygon_type, polygon_type>("flower20", flower, join_round, end_flat, 74.702, 0.20);
    test_one<polygon_type, polygon_type>("flower25", flower, join_round, end_flat, 78.071, 0.25);
    test_one<polygon_type, polygon_type>("flower30", flower, join_round, end_flat, 81.352, 0.30);
    test_one<polygon_type, polygon_type>("flower35", flower, join_round, end_flat, 84.547, 0.35);
    test_one<polygon_type, polygon_type>("flower40", flower, join_round, end_flat, 87.665, 0.40);
    test_one<polygon_type, polygon_type>("flower45", flower, join_round, end_flat, 90.709, 0.45);
    test_one<polygon_type, polygon_type>("flower50", flower, join_round, end_flat, 93.680, 0.50);
    test_one<polygon_type, polygon_type>("flower55", flower, join_round, end_flat, 96.580, 0.55);
    test_one<polygon_type, polygon_type>("flower60", flower, join_round, end_flat, 99.408, 0.60);

    // Flower - deflated
    test_one<polygon_type, polygon_type>("flower60", flower, join_round, end_flat, 19.3210, -0.60);

    // Saw
    {
        // SQL Server:
        // 68.6258859984014 90.2254986930165 112.799509089077 136.392823913949 161.224547934625 187.427508982734
        //215.063576036522 244.167935815974 274.764905445676 306.878264367143 340.530496138041 375.720107548269
        int const n = 12;
        double expected_round[n] =
            {
                68.6252,  90.222, 112.792, 136.397, 161.230, 187.435,
                215.073, 244.179, 274.779, 306.894, 340.543, 375.734
            };
        double expected_miter[n] =
            {
                70.7706,  98.804, 132.101, 170.661, 214.484, 263.57,
                317.92,  377.532, 442.408, 512.546, 587.948, 668.613
            };

        for (int i = 1; i <= n; i++)
        {
            std::ostringstream out;
            out << "saw_" << i;
            test_one<polygon_type, polygon_type>(out.str(), saw, join_round, end_flat, expected_round[i - 1], double(i) / 2.0, ut_settings(0.1));
            test_one<polygon_type, polygon_type>(out.str(), saw, join_miter, end_flat, expected_miter[i - 1], double(i) / 2.0);
        }
    }

    // Bowl
    {
        // SQL Server values - see query below.
        //1 43.2425133175081 60.0257800296593 78.3497997564532 98.2145746255142 119.620102487345 142.482792724034
        //2 166.499856911107 191.763334982583 218.446279387336 246.615018368511 276.300134755606 307.518458532186

        int const n = 12;
        double expected_round[n] =
            {
                43.2423,  60.025,  78.3477,  98.2109, 119.614, 142.487,
                166.505, 191.77, 218.455, 246.625, 276.312, 307.532
            };
        double expected_miter[n] =
            {
                43.4895,  61.014,  80.5726,  102.166, 125.794, 151.374,
                178.599, 207.443, 237.904, 270.000, 304.0, 340.000
            };

        for (int i = 1; i <= n; i++)
        {
            std::ostringstream out;
            out << "bowl_" << i;
            test_one<polygon_type, polygon_type>(out.str(), bowl, join_round, end_flat, expected_round[i - 1], double(i) / 2.0, ut_settings(0.1));
            test_one<polygon_type, polygon_type>(out.str(), bowl, join_miter, end_flat, expected_miter[i - 1], double(i) / 2.0);
        }
    }

    {
        ut_settings settings(0.0000001);
        test_one<polygon_type, polygon_type>("county1", county1, join_round, end_flat, 0.00114092, 0.01, settings);
        test_one<polygon_type, polygon_type>("county1", county1, join_miter, end_flat, 0.00132859, 0.01,  settings);
        test_one<polygon_type, polygon_type>("county1", county1, join_round, end_flat, 3.94411299566854723e-05, -0.003, settings);
        test_one<polygon_type, polygon_type>("county1", county1, join_miter, end_flat, 3.94301960113807581e-05, -0.003, settings);
    }

    test_one<polygon_type, polygon_type>("parcel1_10", parcel1, join_round, end_flat, 7571.405, 10.0);
    test_one<polygon_type, polygon_type>("parcel1_10", parcel1, join_miter, end_flat, 8207.453, 10.0);
    test_one<polygon_type, polygon_type>("parcel1_20", parcel1, join_round, end_flat, 11648.111, 20.0);
    test_one<polygon_type, polygon_type>("parcel1_20", parcel1, join_miter, end_flat, 14184.022, 20.0);
    test_one<polygon_type, polygon_type>("parcel1_30", parcel1, join_round, end_flat, 16350.488, 30.0);
    test_one<polygon_type, polygon_type>("parcel1_30", parcel1, join_miter, end_flat, 22417.799, 30.0);

    test_one<polygon_type, polygon_type>("parcel2_10", parcel2, join_round, end_flat, 5000.867, 10.0);
    test_one<polygon_type, polygon_type>("parcel2_10", parcel2, join_miter, end_flat, 5091.122, 10.0);
    test_one<polygon_type, polygon_type>("parcel2_20", parcel2, join_round, end_flat, 9049.673, 20.0);
    test_one<polygon_type, polygon_type>("parcel2_20", parcel2, join_miter, end_flat, 9410.691, 20.0);
    test_one<polygon_type, polygon_type>("parcel2_30", parcel2, join_round, end_flat, 13726.528, 30.0);
    test_one<polygon_type, polygon_type>("parcel2_30", parcel2, join_miter, end_flat, 14535.232, 30.0);

    test_one<polygon_type, polygon_type>("parcel3_10", parcel3, join_round, end_flat, 19993.007, 10.0);
    test_one<polygon_type, polygon_type>("parcel3_10", parcel3, join_miter, end_flat, 20024.558, 10.0, ut_settings(0.05)); // MSVC 14 reports 20024.51456, so we increase the tolerance
    test_one<polygon_type, polygon_type>("parcel3_20", parcel3, join_round, end_flat, 34505.837, 20.0);
    test_one<polygon_type, polygon_type>("parcel3_20", parcel3, join_miter, end_flat, 34633.261, 20.0);
    test_one<polygon_type, polygon_type>("parcel3_30", parcel3, join_round, end_flat, 45262.452, 30.0);
    test_one<polygon_type, polygon_type>("parcel3_30", parcel3, join_miter, end_flat, 45567.388, 30.0);

    test_one<polygon_type, polygon_type>("parcel3_bend_5", parcel3_bend, join_round, end_flat, 155.634, 5.0);
    test_one<polygon_type, polygon_type>("parcel3_bend_10", parcel3_bend, join_round, end_flat, 458.454, 10.0);

    // These cases differ a bit based on point order, because piece generation is different in one corner. Tolerance is increased
    test_one<polygon_type, polygon_type>("parcel3_bend_15", parcel3_bend, join_round, end_flat, 918.06, 15.0, ut_settings(0.25));
    test_one<polygon_type, polygon_type>("parcel3_bend_20", parcel3_bend, join_round, end_flat, 1534.64, 20.0, ut_settings(0.25));

    // Parcel - deflated
    test_one<polygon_type, polygon_type>("parcel1_10", parcel1, join_round, end_flat, 1571.9024, -10.0);
    test_one<polygon_type, polygon_type>("parcel1_10", parcel1, join_miter, end_flat, 1473.7325, -10.0);
    test_one<polygon_type, polygon_type>("parcel1_20", parcel1, join_round, end_flat, 209.3579, -20.0);
    test_one<polygon_type, polygon_type>("parcel1_20", parcel1, join_miter, end_flat, 188.4224, -20.0);

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<polygon_type, polygon_type>("nl_part1_2", nl_part1, join_round, end_flat,  1848737356.991, -0.2 * 1000.0);
    test_one<polygon_type, polygon_type>("nl_part1_5", nl_part1, join_round, end_flat,  1775953811.679, -0.5 * 1000.0);
#else
    test_one<polygon_type, polygon_type>("nl_part1_2", nl_part1, join_round, end_flat,  1848737292.653, -0.2 * 1000.0);
    test_one<polygon_type, polygon_type>("nl_part1_5", nl_part1, join_round, end_flat,  1775953824.799, -0.5 * 1000.0);
#endif

#if ! defined(BOOST_GEOMETRY_NO_ROBUSTNESS)
    test_one<polygon_type, polygon_type>("italy_part1_30", italy_part1, join_round, end_flat,  5015638814.956, 30.0 * 1000.0);
    test_one<polygon_type, polygon_type>("italy_part1_50", italy_part1, join_round, end_flat, 11363180044.822, 50.0 * 1000.0);
#else
    test_one<polygon_type, polygon_type>("italy_part1_30", italy_part1, join_round, end_flat,  5015638827.704, 30.0 * 1000.0);
    test_one<polygon_type, polygon_type>("italy_part1_50", italy_part1, join_round, end_flat, 11363180033.564, 50.0 * 1000.0);
#endif
    test_one<polygon_type, polygon_type>("italy_part1_60", italy_part1, join_round, end_flat, 15479097108.720, 60.0 * 1000.0);

    {
        ut_settings settings;
        settings.test_validity = false;

        // Tickets
        test_one<polygon_type, polygon_type>("ticket_10398_1_5", ticket_10398_1, join_miter, end_flat, 494.7192, 0.5, settings);
        test_one<polygon_type, polygon_type>("ticket_10398_1_25", ticket_10398_1, join_miter, end_flat, 697.7798, 2.5, settings);
        {
            // qcc-arm reports 1470.79863681712281
            ut_settings specific = settings;
            specific.tolerance = 0.02;
            test_one<polygon_type, polygon_type>("ticket_10398_1_84", ticket_10398_1, join_miter, end_flat, 1470.8096, 8.4, specific);
        }

        test_one<polygon_type, polygon_type>("ticket_10398_2_45", ticket_10398_2, join_miter, end_flat, 535.4780, 4.5, settings);
        test_one<polygon_type, polygon_type>("ticket_10398_2_62", ticket_10398_2, join_miter, end_flat, 705.2046, 6.2, settings);
        test_one<polygon_type, polygon_type>("ticket_10398_2_73", ticket_10398_2, join_miter, end_flat, 827.3394, 7.3, settings);

        test_one<polygon_type, polygon_type>("ticket_10398_3_12", ticket_10398_3, join_miter, end_flat, 122.9443, 1.2, settings);
        test_one<polygon_type, polygon_type>("ticket_10398_3_35", ticket_10398_3, join_miter, end_flat, 258.2729, 3.5, settings);
        test_one<polygon_type, polygon_type>("ticket_10398_3_54", ticket_10398_3, join_miter, end_flat, 402.0571, 5.4, settings);

        test_one<polygon_type, polygon_type>("ticket_10398_4_30", ticket_10398_4, join_miter, end_flat, 257.9482, 3.0, settings);
        test_one<polygon_type, polygon_type>("ticket_10398_4_66", ticket_10398_4, join_miter, end_flat, 553.0112, 6.6, settings);
        test_one<polygon_type, polygon_type>("ticket_10398_4_91", ticket_10398_4, join_miter, end_flat, 819.1406, 9.1, settings);

        test_one<polygon_type, polygon_type>("ticket_10412", ticket_10412, join_miter, end_flat, 3109.6616, 1.5, settings);
        test_one<polygon_type, polygon_type>("ticket_11580_100", ticket_11580, join_miter, end_flat, 52.0221000, 1.00, settings);
    #ifdef BOOST_GEOMETRY_TEST_INCLUDE_FAILING_TESTS
        // Larger distance, resulting in only one circle
        test_one<polygon_type, polygon_type>("ticket_11580_237", ticket_11580, join_miter, end_flat, 999.999, 2.37, settings);
    #endif
    }

    // Tickets - deflated
    test_one<polygon_type, polygon_type>("ticket_10398_1_5", ticket_10398_1, join_miter, end_flat, 404.3936, -0.5);
    test_one<polygon_type, polygon_type>("ticket_10398_1_25", ticket_10398_1, join_miter, end_flat, 246.7329, -2.5);

    {
        bg::strategy::buffer::join_round join_round32(32);
        bg::strategy::buffer::end_round end_round32(32);
        test_one<polygon_type, polygon_type>("mysql_report_2014_10_24", mysql_report_2014_10_24,
            join_round32, end_round32, 174.902, 1.0);
        test_one<polygon_type, polygon_type>("mysql_report_2014_10_28_1", mysql_report_2014_10_28_1,
            join_round32, end_round32, 75.46, 1.0);
        test_one<polygon_type, polygon_type>("mysql_report_2014_10_28_2", mysql_report_2014_10_28_2,
            join_round32, end_round32, 69.117, 1.0);
        test_one<polygon_type, polygon_type>("mysql_report_2014_10_28_3", mysql_report_2014_10_28_3,
            join_round32, end_round32, 63.121, 1.0);

        test_one<polygon_type, polygon_type>("mysql_report_2015_02_17_1_d1",
            mysql_report_2015_02_17_1,
            join_round32, end_round32, 48.879, -1);
        test_one<polygon_type, polygon_type>("mysql_report_2015_02_17_1_d5",
            mysql_report_2015_02_17_1,
            join_round32, end_round32, 0.0, -5.0);
        test_one<polygon_type, polygon_type>("mysql_report_2015_02_17_1_d6",
            mysql_report_2015_02_17_1,
            join_round32, end_round32, 0.0, -6.0);
        test_one<polygon_type, polygon_type>("mysql_report_2015_02_17_1_d10",
            mysql_report_2015_02_17_1,
            join_round32, end_round32, 0.0, -10.0);

        test_one<polygon_type, polygon_type>("mysql_report_2015_02_17_2_d1",
            mysql_report_2015_02_17_2,
            join_round32, end_round32, 64.0, -1.0);
        test_one<polygon_type, polygon_type>("mysql_report_2015_02_17_2_d10",
            mysql_report_2015_02_17_2,
            join_round32, end_round32, 0.0, -10.0);
        test_one<polygon_type, polygon_type>("mysql_report_2015_02_17_3_d1",
            mysql_report_2015_02_17_3,
            join_round32, end_round32, 64.0, -1.0);

        {
            // These extreme testcases, containing huge coordinate differences
            // and huge buffer distances, are to verify assertions.
            // No assertions should be raised.

            // The buffers themselves are most often wrong. Versions
            // without interior rings might be smaller (or have no output)
            // then their versions with interior rings.

            // Therefore all checks on area, validity, self-intersections
            // or having output at all are omitted.

            // Details (for versions with interior rings)
            // Case 3 is reported as invalid
            // On MinGW, also case 2 and 4 are reported as invalid
            // On PowerPC, also case 1 is reported as invalid

            ut_settings settings = ut_settings::assertions_only();
            const double no_area = ut_settings::ignore_area();

            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_0", mysql_report_2015_07_05_0,
                join_round32, end_round32, no_area, 6.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_1", mysql_report_2015_07_05_1,
                join_round32, end_round32, no_area, 6.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_2", mysql_report_2015_07_05_2,
                join_round32, end_round32, no_area, 549755813889.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_3", mysql_report_2015_07_05_3,
                join_round32, end_round32, no_area, 49316.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_4", mysql_report_2015_07_05_4,
                join_round32, end_round32, no_area, 1479986.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_5", mysql_report_2015_07_05_5,
                join_round32, end_round32, no_area, 38141.0, settings);

            // Versions without interior rings (area should be smaller but still no checks)
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_0_wi", mysql_report_2015_07_05_0_wi,
                join_round32, end_round32, no_area, 6.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_1_wi", mysql_report_2015_07_05_1_wi,
                join_round32, end_round32, no_area, 6.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_2_wi", mysql_report_2015_07_05_2_wi,
                join_round32, end_round32, no_area, 549755813889.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_3_wi", mysql_report_2015_07_05_3_wi,
                join_round32, end_round32, no_area, 49316.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_4_wi", mysql_report_2015_07_05_4_wi,
                join_round32, end_round32, no_area, 1479986.0, settings);
            test_one<polygon_type, polygon_type>("mysql_report_2015_07_05_5_wi", mysql_report_2015_07_05_5_wi,
                join_round32, end_round32, no_area, 38141.0, settings);
        }
    }


    {
        using bg::strategy::buffer::join_round;
        using bg::strategy::buffer::join_miter;
        bg::strategy::buffer::side_straight side_strategy;
        bg::strategy::buffer::point_circle point_strategy;
        typedef bg::strategy::buffer::distance_symmetric
        <
            typename bg::coordinate_type<P>::type
        > distance;

        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_j12",
                sharp_triangle,
                join_round(12), end_flat, distance(1.0), side_strategy, point_strategy,
                29.1604);
        // Test very various number of points (min is 3)
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_j2",
                sharp_triangle,
                join_round(2), end_flat, distance(1.0), side_strategy, point_strategy,
                27.2399);
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_j5",
                sharp_triangle,
                join_round(5), end_flat, distance(1.0), side_strategy, point_strategy,
                28.8563);

        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_j36",
                sharp_triangle,
                join_round(36), end_flat, distance(1.0), side_strategy, point_strategy,
                29.2482);
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_j360",
                sharp_triangle,
                join_round(360), end_flat, distance(1.0), side_strategy, point_strategy,
                29.2659);

        // Test with various miter limits
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_m2",
                sharp_triangle,
                join_miter(2), end_flat, distance(4.0), side_strategy, point_strategy,
                148.500);
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_m3",
                sharp_triangle,
                join_miter(3), end_flat, distance(4.0), side_strategy, point_strategy,
                164.376);
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_m4",
                sharp_triangle,
                join_miter(4), end_flat, distance(4.0), side_strategy, point_strategy,
                180.2529);
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_m5",
                sharp_triangle,
                join_miter(5), end_flat, distance(4.0), side_strategy, point_strategy,
                196.1293);
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_m25",
                sharp_triangle,
                join_miter(25), end_flat, distance(4.0), side_strategy, point_strategy,
                244.7471);

        // Right triangles, testing both points around sharp corner as well as points
        // around right corners in join_round strategy
        test_with_custom_strategies<polygon_type, polygon_type>("right_triangle_j3",
                right_triangle,
                join_round(3), end_flat, distance(1.0), side_strategy, point_strategy,
                53.0240);
        test_with_custom_strategies<polygon_type, polygon_type>("right_triangle_j4",
                right_triangle,
                join_round(4), end_flat, distance(1.0), side_strategy, point_strategy,
                53.2492);
        test_with_custom_strategies<polygon_type, polygon_type>("right_triangle_j5",
                right_triangle,
                join_round(5), end_flat, distance(1.0), side_strategy, point_strategy,
                53.7430);
        test_with_custom_strategies<polygon_type, polygon_type>("right_triangle_j6",
                right_triangle,
                join_round(6), end_flat, distance(1.0), side_strategy, point_strategy,
                53.7430);

        buffer_custom_side_strategy custom_side_strategy;
        test_with_custom_strategies<polygon_type, polygon_type>("sharp_triangle_custom_side",
                sharp_triangle,
                join_round(49), end_flat, distance(1.0), custom_side_strategy, point_strategy,
                31.1087);
    }
}

template <bool Clockwise, typename P>
void test_deflate_special_cases()
{
    typedef bg::model::polygon<P, Clockwise, true> polygon_type;

    bg::strategy::buffer::join_miter join_miter(5);
    bg::strategy::buffer::join_round join_round(8);
    bg::strategy::buffer::end_flat end_flat;

    // This case fails for <float> because there is an IP formed at the border of the original
    test_one<polygon_type, polygon_type>("erode_50", erode_triangle, join_miter, end_flat, 0, 0, 0.0, -0.50);
    test_one<polygon_type, polygon_type>("erode_40", erode_triangle, join_miter, end_flat, 0, 0, 0.0, -0.40);
    test_one<polygon_type, polygon_type>("erode_60", erode_triangle, join_miter, end_flat, 0, 0, 0.0, -0.60);

    // This case generates a uu-turn in deflate, at 1.0
    test_one<polygon_type, polygon_type>("forming_uu_a_95", forming_uu_a, join_round, end_flat, 1, 0, 23.0516, -0.95);
    test_one<polygon_type, polygon_type>("forming_uu_a_10", forming_uu_a, join_round, end_flat, 2, 0, 21.4606, -1.0);
    test_one<polygon_type, polygon_type>("forming_uu_a_15", forming_uu_a, join_round, end_flat, 2, 0, 8.8272, -1.5);
    test_one<polygon_type, polygon_type>("forming_uu_a_20", forming_uu_a, join_round, end_flat, 2, 0, 1.7588, -2.0);
    test_one<polygon_type, polygon_type>("forming_uu_a_21", forming_uu_a, join_round, end_flat, 2, 0, 0.9944, -2.1);

    test_one<polygon_type, polygon_type>("forming_uu_b_25", forming_uu_b, join_round, end_flat, 1, 1, 38.4064, -0.25);
    test_one<polygon_type, polygon_type>("forming_uu_b_50", forming_uu_b, join_round, end_flat, 1, 1, 24.4551, -0.50);
    test_one<polygon_type, polygon_type>("forming_uu_b_95", forming_uu_b, join_round, end_flat, 1, 0, 11.5009, -0.95);
    test_one<polygon_type, polygon_type>("forming_uu_b_10", forming_uu_b, join_round, end_flat, 1, 0, 10.7152, -1.0);
    test_one<polygon_type, polygon_type>("forming_uu_b_15", forming_uu_b, join_round, end_flat,  1, 0, 4.4136, -1.5);

    test_one<polygon_type, polygon_type>("forming_uu_b_25", forming_uu_b, join_round, end_flat, 1, 1, 67.7139, 0.25);
    test_one<polygon_type, polygon_type>("forming_uu_b_50", forming_uu_b, join_round, end_flat, 1, 1, 82.0260, 0.50);
    test_one<polygon_type, polygon_type>("forming_uu_b_70", forming_uu_b, join_round, end_flat, 1, 3, 93.0760, 0.70);

    // From here on a/b have the same result
    test_one<polygon_type, polygon_type>("forming_uu_a_10", forming_uu_a, join_round, end_flat, 1, 0, 108.0959, 1.0);
    test_one<polygon_type, polygon_type>("forming_uu_b_10", forming_uu_b, join_round, end_flat, 1, 0, 108.0959, 1.0);
}

template
<
    typename InputPoint,
    typename OutputPoint,
    bool InputClockwise,
    bool OutputClockwise,
    bool InputClosed,
    bool OutputClosed
>
void test_mixed()
{
    typedef bg::model::polygon<InputPoint, InputClockwise, InputClosed> input_polygon_type;
    typedef bg::model::polygon<OutputPoint, OutputClockwise, OutputClosed> output_polygon_type;

    bg::strategy::buffer::join_round join_round(12);
    bg::strategy::buffer::end_flat end_flat;

    std::ostringstream name;
    name << "mixed_" << std::boolalpha
        << InputClockwise << "_" << OutputClockwise
        << "_" << InputClosed << "_" << OutputClosed;

    test_one<input_polygon_type, output_polygon_type>(name.str(),
            simplex, join_round, end_flat, 47.4831, 1.5);
}

#ifdef HAVE_TTMATH
#include <ttmath_stub.hpp>
#endif

int test_main(int, char* [])
{
    typedef bg::model::point<double, 2, bg::cs::cartesian> dpoint;

    test_all<true, dpoint>();
    test_all<false, dpoint>();

    typedef bg::model::point<float, 2, bg::cs::cartesian> fpoint;
    test_deflate_special_cases<true, fpoint>();
    test_deflate_special_cases<true, dpoint>();

#if ! defined(BOOST_GEOMETRY_TEST_ONLY_ONE_TYPE)

    test_mixed<dpoint, dpoint, false, false, true, true>();
    test_mixed<dpoint, dpoint, false, true, true, true>();
    test_mixed<dpoint, dpoint, true, false, true, true>();
    test_mixed<dpoint, dpoint, true, true, true, true>();

    test_mixed<dpoint, dpoint, false, false, false, true>();
    test_mixed<dpoint, dpoint, false, true, false, true>();
    test_mixed<dpoint, dpoint, true, false, false, true>();
    test_mixed<dpoint, dpoint, true, true, false, true>();

#ifdef HAVE_TTMATH
    test_all<bg::model::point<tt, 2, bg::cs::cartesian> >();
#endif

#endif

    return 0;
}
