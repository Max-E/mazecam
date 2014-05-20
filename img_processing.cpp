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

static void WmorphologyEx (Mat src, Mat dst, int op, InputArray kernel)
{
    morphologyEx (src, dst, op, kernel);
}

// Parallel morphologyEx function
static void PmorphologyEx (Mat& src, Mat& dst, int op, InputArray kernel)
{
#define THREADS 2
    
    if (THREADS == 1)
        return morphologyEx (src, dst, op, kernel);
    
    int size = src.size().height/THREADS;
    int margin_bottom, margin_top;
    Mat out_section_margin[THREADS];
    thread threads[THREADS];
    
    margin_bottom = margin_top = kernel.size().height / 2;
    
    dst = Mat (src.size(), src.type());
    
    for (int i = 0; i < THREADS; i++)
    {
        int start = i*size, end = (i+1)*size;
        if (i)
            start -= margin_top;
        if (i+1 < THREADS)
            end += margin_bottom;
        Mat src_section_margin = src (Range(start, end), Range::all());
        out_section_margin[i] = Mat (src_section_margin.size(), src_section_margin.type());
        if (i < THREADS-1)
            threads[i] = thread (WmorphologyEx, src_section_margin, out_section_margin[i], op, kernel);
        else
            WmorphologyEx (src_section_margin, out_section_margin[i], op, kernel);
    }
    
    for (int i = 0; i < THREADS; i++)
    {
        int keep_start = 0;
        if (i)
            keep_start = margin_top;
        if (i < THREADS-1)
            threads[i].join ();
        Mat out_src = out_section_margin[i] (Range(keep_start, keep_start + size), Range::all());
        Mat out_dst = dst (Range (i*size, (i+1)*size), Range::all());
        out_src.copyTo (out_dst);
    }
    
#ifdef VERIFY
    Mat dst2 = Mat (src.size(), src.type());
    morphologyEx (src, dst2, op, kernel);
    cout << equal(dst.begin<uchar>(), dst.end<uchar>(), dst2.begin<uchar>()) << endl;
#endif
}

const double arrow_scale = 15;

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

/** @function do_process */
void do_process (Mat process_in)
{
    Mat writing_emphasized;
    vector<vector<Point> > contours_unfiltered, contours;
    vector<Vec4i> hierarchy;

    // Find the writing in the image. TODO: more efficient method.
    #define GETELEMENT(sz) getStructuringElement(2, Size( 2*sz + 1, 2*sz+1 ), Point( sz, sz ) )
    Mat dst;
    PmorphologyEx (process_in, dst, MORPH_BLACKHAT, GETELEMENT(15));
    threshold (dst, dst, 5, 0, 3);
    equalizeHist (dst, dst);
    threshold (dst, dst, 16, 255, 0);
    dst.copyTo (writing_emphasized);
    
    /// Find contours
    findContours (writing_emphasized, contours_unfiltered, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point (0, 0));
    for (auto i = contours_unfiltered.begin (); i != contours_unfiltered.end (); i++)
    {
        vector<Point> tmp_contour;
        approxPolyDP (*i, tmp_contour, 2, true);
        double area = contourArea (tmp_contour);
         if (area > 150 && area < 25600)
            contours.push_back (tmp_contour);
    }

    // Isolate all the arrows
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
    
    // Draw a visualization of what the image processing algorithm sees
    Mat drawing = Mat::zeros( writing_emphasized.size(), CV_8UC3 );
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

    /// Show in a window
    namedWindow( "Contours", CV_WINDOW_AUTOSIZE );
    imshow( "Contours", drawing );
    process_done = true;
}
