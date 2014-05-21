#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include "mazedemo_common.h"

using namespace cv;
using namespace std;

// I hate C++
bool process_done; 
arrowvec_t process_output; 

const double arrow_scale = 8;

// Sort arrows right to left, top to bottom.
static bool compare_arrow (arrow_t a, arrow_t b)
{
    // Check if they're in different rows first
    if (a.vert_max - 0.15 * (a.vert_max - a.vert_min) < b.vert_min)
        return true;
    
    if (a.vert_min > b.vert_max - 0.15 * (b.vert_max - b.vert_min))
        return false;
    
    return a.origin[0] < b.origin[0];
}

static void draw_arrow (Mat &canvas, Scalar color, Point2f &org, Point2f &axis, Point2f &ortho_axis)
{
    #define DRAWPT(x,y) org + arrow_scale * ((x * axis) + (y * ortho_axis))
    Point2f arrow_start = DRAWPT (-1, 0),
            arrow_tip = DRAWPT (1, 0),
            arrow_left = DRAWPT (0.5, 0.5), 
            arrow_right = DRAWPT (0.5, -0.5);
    line (canvas, arrow_start, arrow_tip, color, 2, CV_AA);
    line (canvas, arrow_tip, arrow_right, color, 2, CV_AA);
    line (canvas, arrow_tip, arrow_left, color, 2, CV_AA);
    line (canvas, arrow_right, arrow_left, color, 2, CV_AA);
}

RNG rng(12345);

/** @function do_process */
void do_process (Mat process_in)
{
    vector<vector<Point> > contours_unfiltered, contours;
    vector<Vec4i> hierarchy;

    // Use the Canny edge-detect filter, then dilate the image to merge the 
    // detected edges a bit.
    Mat canny_out;
    Canny (process_in, canny_out, 30, 60, 3);
    #define GETELEMENT(sz) getStructuringElement(2, Size( 2*sz + 1, 2*sz+1 ), Point( sz, sz ) )
    dilate (canny_out, canny_out, GETELEMENT(3));
    
    // Find contours and isolate the ones we're interested in
    findContours (canny_out, contours_unfiltered, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);
    for (auto i = contours_unfiltered.begin (); i != contours_unfiltered.end (); i++)
    {
        // Simplify the contour for efficiency
        vector<Point> tmp_contour;
        approxPolyDP (*i, tmp_contour, 2, true);
        
        // Filter out the contours in the wrong size range
        double area = contourArea (tmp_contour);
        if (area < 800 || area > 25600)
            continue;
        
        // The contour detection algorithm generates contours around dark 
        // patches, but also around light patches (i.e. spots of specular
        // lighting or glare.) We're interested in the contours that enclose 
        // darker regions, so filter by the average pixel intensity of the
        // contour's bounding box. Method from
        // http://stackoverflow.com/questions/17936510
        Rect roi = boundingRect (tmp_contour);
        if (mean (process_in (roi)).val[0] > 180)
            continue;
        
        contours.push_back (tmp_contour);
    }

    // Assign an arrow origin and direction to each contour
    process_output.clear ();
    for (int i = 0; i < contours.size (); i++)
    {
        // Determine the arrow's axis (vertical or horizontal)
        double vertweight = 0, horizweight = 0;
        for (int j = 0; j < contours[i].size() - 1; j++)
        {
            Point2f diff = contours[i][j+1] - contours[i][j];
            double weight = norm(diff);
            if (fabs (diff.x) > fabs (diff.y))
                horizweight += weight;
            else
                vertweight += weight;
        }
        
        bool vert;
        if (vertweight > 1.0*horizweight)
            vert = true;
        else if (horizweight > 1.0*vertweight)
            vert = false;
        else
            continue;
        
        // Get center of mass
        Moments mu = moments (contours[i], false);
        Point2f mc = Point2f( mu.m10/mu.m00 , mu.m01/mu.m00 );
        
        // Get center of bounding box
        Rect br = boundingRect( Mat(contours[i]) );
        Point2f bc = (br.tl () + br.br ()) * 0.5;
        
        // determine the arrow's direction (left/right, up/down)
        bool dir = (vert && mc.y > bc.y) || (!vert && mc.x > bc.x);
        
        arrow_t new_arrow;
        new_arrow.contour_num = i;
        new_arrow.dir = vert?(dir?arrow_down:arrow_up):(dir?arrow_right:arrow_left);
        new_arrow.origin[0] = mc.x;
        new_arrow.origin[1] = mc.y;
        new_arrow.vert_min = br.tl ().y;
        new_arrow.vert_max = br.br ().y;
        process_output.push_back (new_arrow);
    }
    
    // Organize arrows into rows and columns
    sort (process_output.begin (), process_output.end (), compare_arrow);
    
    // Rescale contours down to fit drawing area
    for (auto i = contours.begin (); i != contours.end (); i++)
    {
        for (auto j = i->begin (); j != i->end (); j++)
            *j *= 0.5;
    }
    
    // Draw a visualization of what the image processing algorithm sees
    Mat drawing = processing_visualization_area;
    drawing.setTo (Scalar (0, 0, 0));
    Point2f cursor (arrow_scale, arrow_scale);
    int n = 0;
    for (auto i = process_output.begin (); i != process_output.end (); i++, n++)
    {
        Point2f axis (0, 0), ortho_axis (0, 0);
        if (i->dir == arrow_up || i->dir == arrow_down)
            axis.y = ortho_axis.x = i->dir == arrow_down ? 1 : -1;
        else
            axis.x = ortho_axis.y = i->dir == arrow_right ? 1 : -1;
        Point2f org (i->origin[0], i->origin[1]);
        org *= 0.5;
        
        Scalar color = Scalar (fabs(axis.x)*255, (axis.x+axis.y)*255, fabs(axis.y)*255);
        
        // Draw a dot at the center of the arrow
        circle (drawing, org, 4, color, -1, 8, 0);
        
        // Draw an outline of the drawn object
        drawContours (drawing, contours, i->contour_num, color, 1, 8, hierarchy, 0, Point());
        
        // Draw what type of arrow we think the drawn object is over the drawn
        // object itself
        draw_arrow (drawing, Scalar(255,255,255), org, axis, ortho_axis);
        
        // Draw a number representing which arrow this is over
        putText (drawing, to_string (n), org, 0, 0.8, Scalar (0,255,0));
        
        // Draw the arrow type into the "paragraph" so we know what order it's
        // reading them in
        draw_arrow (drawing, Scalar(0,255,0), cursor, axis, ortho_axis);
        cursor.x += 2.5*arrow_scale;
        
        if (cursor.x >= drawing.size().width)
        {
            cursor.y += 2.5*arrow_scale;
            cursor.x = arrow_scale;
        }
    }

    process_done = true;
}
