#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include "mazedemo_common.h"

using namespace cv;
using namespace std;

// Measured in grid cells
#define maze_side 8

void draw_maze_lines (Mat &canvas, mazepublic_t &maze, Scalar color)
{
    double scale = canvas.size ().height/(maze_side+2)/2;
    for (int linenum = 0; linenum < maze.numlines; linenum++)
    {
        Point2f start (maze.lines[linenum][0][0]+2, maze.lines[linenum][0][1]+2);
        Point2f end (maze.lines[linenum][1][0]+2, maze.lines[linenum][1][1]+2);
        line (canvas, scale*start, scale*end, color, 2, CV_AA);
    }
}

Mat processing_visualization_area;

int main (int argc, char *argv[])
{
    if (!camera_setup ())
        return 0;
    
    char* source_window = "Source";
    namedWindow (source_window, CV_WINDOW_NORMAL);
    Mat display (Size (cfg_w/2+cfg_h, cfg_h), CV_8UC3);
    resizeWindow (source_window, cfg_w/2+cfg_h, cfg_h);

    Mat camera_display_area = display (Rect (0, 0, cfg_w/2, cfg_h/2));
    processing_visualization_area = display (Rect (0, cfg_h/2, cfg_w/2, cfg_h/2));
    Mat maze_display_area = display (Rect (cfg_w/2, 0, cfg_h, cfg_h));
    
    Mat process_in;
    process_done = true;
    thread worker;
    
    mazepublic_t maze, trace;
    generate_maze (8, 8, &maze);
    memset (&trace, 0, sizeof(trace));
    
    int i = 0;
    while (true)
    {
        i++;
        Mat src = camera_getframe ();
        resize (src, camera_display_area, camera_display_area.size ());

        // Convert image to gray and blur it
        Mat src_gray;
        cvtColor (src, src_gray, CV_BGR2GRAY);
        
        // Preview camera output
        imshow (source_window, display);
        
#define WORKER_THREAD
#ifdef WORKER_THREAD
        if (process_done)
        {
            if (worker.joinable ())
                worker.join ();
            maze_display_area.setTo (Scalar (0, 0, 0));
            bool victory = maze_trace (process_output.size(), &process_output[0], &trace);
            draw_maze_lines (maze_display_area, maze, Scalar (0, 255, 0));
            draw_maze_lines (maze_display_area, trace, Scalar (0, 0, 255));
            if (victory)
            {
                putText (maze_display_area, "MAZE SOLVED (any key to play again)", Point (60, 60), 0, 2.0, Scalar (0,255,255), 3, CV_AA);
                imshow (source_window, display);
                waitKey (0);
                // Flush a couple of frames that the webcam might have buffered
                for (int j = 0; j < 5; j++)
                    camera_getframe ();
                generate_maze (8, 8, &maze);
                memset (&trace, 0, sizeof(trace));
            }
            process_done = false;
            src_gray.copyTo (process_in);
            worker = thread (do_process, process_in);
        }
#else
        src_gray.copyTo (process_in);
        if ((i%2) == 0)
        {
            do_process (process_in);
            maze_trace (process_output.size(), &process_output[0], &trace);
        }
#endif
        
        waitKey(1);
        //if (i == 100) break;
    }
    
#ifdef WORKER_THREAD
    worker.join ();
#endif

    return(0);
}
