#include <stdlib.h>
#pragma once
#define COLOR_PALETTE_NUM 2

#define __S(x) x / 255.0f //[0..255]->[0..1] scaling operation

//The palettes are defined in [0..255] range 
//and then scaled down to [0..1] range
#if COLOR_PALETTE_NUM==0
#define BALL_COLOR_TOP __S(242.0f), __S(241.0f), __S(232.0f), 1.0f
#define BALL_COLOR_BOTTOM __S(242.0f), __S(241.0f), __S(232.0f), 1.0f

#define BALL_SHADOW_COLOR_TOP __S(38.2f), __S(75.0f), __S(104.2f), 0.2f
#define BALL_SHADOW_COLOR_BOTTOM __S(0.2f), __S(37.0f), __S(66.2f), 0.9f

#define OBSTACLE_COLOR_TOP __S(13.0), __S(105.0f), __S(139.0f), 1.0f
#define OBSTACLE_COLOR_LEFT __S(5.0f), __S(5.0f), __S(31.0f), 1.0f
#define OBSTACLE_COLOR_RIGHT __S(5.0f), __S(5.0f), __S(31.0f), 1.0f
#define OBSTACLE_COLOR_BOTTOM __S(5.0f), __S(5.0f), __S(31.0f), 1.0f

#define TRACK_COLOR_DARK __S(5.0f), __S(5.0f), __S(31.0f), 1.0f
#define TRACK_COLOR_LIGHT __S(13.0), __S(105.0f), __S(139.0f), 1.0f
#define TRACK_LINES_COLOR __S(242.0f), __S(241.0f), __S(232.0f), 1.0f
#elif COLOR_PALETTE_NUM==1
#define BALL_COLOR_TOP __S(253.0), __S(183.0f), __S(80.0f), 1.0f
#define BALL_COLOR_BOTTOM __S(253.0), __S(183.0f), __S(80.0f), 1.0f

#define BALL_SHADOW_COLOR_TOP __S(123.0), __S(53.0f), __S(0.0f), 0.2f
#define BALL_SHADOW_COLOR_BOTTOM __S(123.0), __S(53.0f), __S(0.0f), 0.9f

#define OBSTACLE_COLOR_TOP __S(252.0f), __S(46.0f), __S(32.0f), 1.0f
#define OBSTACLE_COLOR_LEFT __S(252.0f), __S(46.0f), __S(32.0f), 1.0f
#define OBSTACLE_COLOR_RIGHT __S(252.0f), __S(46.0f), __S(32.0f), 1.0f
#define OBSTACLE_COLOR_BOTTOM __S(253.0f), __S(127.0f), __S(32.0f), 1.0f

#define TRACK_COLOR_DARK __S(1.0f), __S(1.0f), __S(0.0f), 1.0f
#define TRACK_COLOR_LIGHT __S(10.0f), __S(5.0f), __S(9.0f), 1.0f
#define TRACK_LINES_COLOR __S(253.0), __S(183.0f), __S(80.0f), 1.0f
#elif COLOR_PALETTE_NUM==2
#define BALL_COLOR_TOP __S(213.0f), __S(229.0f), __S(255.0f), 1.0f
#define BALL_COLOR_BOTTOM __S(237.0f), __S(238.0f), __S(241.0f), 1.0f

#define BALL_SHADOW_COLOR_TOP __S(74.0f), __S(72.8f), __S(120.8f), 0.2f
#define BALL_SHADOW_COLOR_BOTTOM __S(54.0f), __S(52.8f), __S(100.8f), 0.9f

#define OBSTACLE_COLOR_TOP __S(165.0f), __S(124.0f), __S(180.0f), 1.0f
#define OBSTACLE_COLOR_LEFT __S(50.0f), __S(66.0f), __S(102.0f), 1.0f
#define OBSTACLE_COLOR_RIGHT __S(50.0f), __S(66.0f), __S(102.0f), 1.0f
#define OBSTACLE_COLOR_BOTTOM __S(50.0f), __S(66.0f), __S(102.0f), 1.0f

#define TRACK_COLOR_DARK __S(60.0f), __S(76.0f), __S(112.0f), 1.0f
#define TRACK_COLOR_LIGHT __S(145.0f), __S(104.0f), __S(160.0f), 1.0f
#define TRACK_LINES_COLOR __S(237.0f), __S(238.0f), __S(241.0f), 1.0f
#elif COLOR_PALETTE_NUM==3
#define BALL_COLOR_TOP __S(242.0f), __S(240.0f), __S(252.0f), 1.0f
#define BALL_COLOR_BOTTOM __S(242.0f), __S(240.0f), __S(252.0f), 1.0f

#define BALL_SHADOW_COLOR_TOP __S(0.0f), __S(99.0f), __S(170.0f), 0.2f
#define BALL_SHADOW_COLOR_BOTTOM __S(0.0f), __S(69.0f), __S(140.0f), 0.9f

#define OBSTACLE_COLOR_TOP __S(235.0f), __S(0.0f), __S(0.0f), 1.0f
#define OBSTACLE_COLOR_LEFT __S(7.0f), __S(119.0f), __S(190.0f), 1.0f
#define OBSTACLE_COLOR_RIGHT __S(7.0f), __S(119.0f), __S(190.0f), 1.0f
#define OBSTACLE_COLOR_BOTTOM __S(7.0f), __S(119.0f), __S(190.0f), 1.0f

#define TRACK_COLOR_DARK __S(7.0f), __S(119.0f), __S(190.0f), 1.0f
#define TRACK_COLOR_LIGHT  __S(17.0f), __S(129.0f), __S(200.0f), 1.0f
#define TRACK_LINES_COLOR __S(242.0f), __S(240.0f), __S(252.0f), 1.0f
#elif COLOR_PALETTE_NUM==4
#define BALL_COLOR_TOP __S(251.0f), __S(220.0f), __S(106.0f), 1.0f
#define BALL_COLOR_BOTTOM __S(251.0f), __S(220.0f), __S(106.0f), 1.0f

#define BALL_SHADOW_COLOR_TOP __S(92.6), __S(38.0f), __S(30.0f), 0.2f
#define BALL_SHADOW_COLOR_BOTTOM __S(72.6), __S(8.0f), __S(10.0f), 0.9f

#define OBSTACLE_COLOR_TOP __S(39.0f), __S(10.0f), __S(14.0f), 1.0f
#define OBSTACLE_COLOR_LEFT __S(39.0f), __S(10.0f), __S(14.0f), 1.0f
#define OBSTACLE_COLOR_RIGHT __S(39.0f), __S(10.0f), __S(14.0f), 1.0f
#define OBSTACLE_COLOR_BOTTOM __S(177.0f), __S(43.0f), __S(36.0f), 1.0f

#define TRACK_COLOR_DARK  __S(9.0f), __S(10.0f), __S(14.0f), 1.0f
#define TRACK_COLOR_LIGHT __S(118.0f), __S(5.0f), __S(4.0f), 1.0f
#define TRACK_LINES_COLOR __S(251.0f), __S(220.0f), __S(106.0f), 1.0f
#endif 