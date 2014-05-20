#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include "mazedemo_common.h"

using namespace cv;
using namespace std;

void draw_maze_lines (Mat &canvas, mazepublic_t &maze, double scale, Scalar color)
{
    for (int linenum = 0; linenum < maze.numlines; linenum++)
    {
        Point2f start (maze.lines[linenum][0][0]+1, maze.lines[linenum][0][1]+1);
        Point2f end (maze.lines[linenum][1][0]+1, maze.lines[linenum][1][1]+1);
        line (canvas, scale*start, scale*end, color, 2, CV_AA);
    }
}

void render_with_maze (char *window_id, Mat &src, mazepublic_t &maze, mazepublic_t &trace, bool victory)
{
    Mat with_maze;
    src.copyTo (with_maze);
    
    draw_maze_lines (with_maze, maze, 50, Scalar (0,255,0));
    draw_maze_lines (with_maze, trace, 50, Scalar (0,0,255));

    if (victory)
        putText (with_maze, "MAZE SOLVED (any key to play again)", Point (60, 60), 0, 2.0, Scalar (0,255,255), 3, CV_AA);
    
    imshow (window_id, with_maze);
    
    if (victory)
        waitKey (0);
}

int main (int argc, char *argv[])
{
    if (!camera_setup ())
        return 0;
    
    char* source_window = "Source";
    namedWindow (source_window, CV_WINDOW_AUTOSIZE);
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

        // Convert image to gray and blur it
        Mat src_gray;
        cvtColor (src, src_gray, CV_BGR2GRAY);
        blur (src_gray, src_gray, Size(6,6));
        
        // Preview camera output
        render_with_maze (source_window, src, maze, trace, false);
        
#define WORKER_THREAD
#ifdef WORKER_THREAD
        if (process_done)
        {
            if (worker.joinable ())
                worker.join ();
            if (maze_trace (process_output.size(), &process_output[0], &trace)) 
            {
                render_with_maze (source_window, src, maze, trace, true);
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
