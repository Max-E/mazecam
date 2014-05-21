#ifdef __cplusplus

// Webcam defaults & scaling information

// This is the resolution at which all the magic numbers were calibrated
const int base_w = 1280;
const int base_h = 960;
const int base_area = base_w*base_h;

// This is the resolution at which the camera is configured
const int cfg_w = 1280;
const int cfg_h = 720;
const int cfg_area = cfg_w*cfg_h;

// Magic number scaling factors
const double scale_len = (double)cfg_h/(double)base_h;
const double scale_area = (double)cfg_area/(double)base_area;

// For communicating with the image-processing worker thread
void do_process (cv::Mat process_in); // worker thread main function
extern bool process_done; // worker thread sets this when it's done with a frame
extern cv::Mat processing_visualization_area;

extern "C" {
#endif

typedef enum {arrow_up, arrow_down, arrow_right, arrow_left} arrowdir_t;
typedef struct 
{
    int contour_num;
    arrowdir_t dir;
    double vert_min, vert_max;
    double origin[2]; 
} arrow_t;

typedef int mazepoint_t[2];
typedef mazepoint_t mazeline_t[2];
typedef struct
{
    int numlines;
    mazeline_t *lines;
} mazepublic_t;

void generate_maze (int width, int height, mazepublic_t *out);
bool maze_trace (int num_arrows, arrow_t *arrows, mazepublic_t *out);

#ifdef __cplusplus
}

typedef std::vector<arrow_t> arrowvec_t;
extern arrowvec_t process_output; // All arrows detected

// Webcam capture functions
bool camera_setup (void); // Returns true on success
cv::Mat camera_getframe (void); 
#endif
