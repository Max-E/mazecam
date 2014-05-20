#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include "mazedemo_common.h"

using namespace cv;
using namespace std;

static CvCapture* capture;

// Returns true on success
bool camera_setup (void)
{
    capture = 0;
    
    capture = cvCaptureFromCAM(0); 
    if (!capture)
    {
        cout << "NO CAMERA" << endl;
        return false;
    }
    
    cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_WIDTH, base_w);
    cvSetCaptureProperty (capture, CV_CAP_PROP_FRAME_HEIGHT, base_h);
    
    return true;
}

Mat camera_getframe (void)
{
     return cvQueryFrame (capture);
}
