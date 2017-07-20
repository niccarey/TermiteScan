/* Program to view and record multiple RealSense streams for imaging purposes */

// include standard libraries
#include <iostream>
#include <cstdio>
#include <string>
#include <sys/resource.h>
#include <map>
#include <atomic>

// include some special libraries
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/gil/gil_all.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <boost/range/adaptors.hpp>
#include <boost/range/algorithm.hpp>

#ifndef BOOST_GIL_NO_IO
#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/io/dynamic_io.hpp>
#endif

#include <fstream>
#include <jpeglib.h>
#include <jpegint.h>
#include "../examples/concurrency.hpp"

// Include the librealsense C++ header file
#include <librealsense/rs.hpp>

// include GLFW for display
#include <GLFW/glfw3.h>

// Don't use QT libraries, they won't work properly

// define crucial constants. Hard to change.
#define COLWIDTH 1920
#define COLHEIGHT 1080
#define COL_SWIDTH 640
#define COL_SHEIGHT 480
#define DEPTHWIDTH 640
#define DEPTHHEIGHT 480
#define FRAMERATE 30
#define FRAMERATE_LIM 28

// global vars (that we might want to change)
unsigned char g_movflag = 0x00;
static float colframerate = 30;
static int col_fps_new = 5;
static int dep_fps_new = 1;
static float depthframerate = 30;
static bool s_movflag = false;
static bool hresflag = false;

namespace bfs = boost::filesystem;
namespace bchrono = boost::chrono;

bool loopflag{false};

// queue for timestamps - has to be very large as we still have significant lag
boost::lockfree::spsc_queue<int, boost::lockfree::capacity<90000> > timestampList;

// paths need to be generally available and modifiable
// this is throwing up a potential memory leak, could be related to segfault issue? unclear
bfs::path cpath{"/home/nic/Documents/TermiteRecord/"};
bfs::path dpath{"/home/nic/Documents/TermiteRecord/"};

// Save functions (called via threading)

void saveColIm(rs::device* dev, bfs::path c_path)
{
    using namespace boost::gil;

    int arraysize = COLWIDTH*COLHEIGHT;
    if (!hresflag) arraysize = COL_SWIDTH*COL_SHEIGHT;

    // color frame number - rarely used in the field but useful for calibration
    static int calib_col_num = 0;

    const void * cpoint = dev->get_frame_data(rs::stream::color);
    //int time_stamp = dev->get_frame_timestamp(rs::stream::color);
    std::string c_file = "ColSnap_" + std::to_string(calib_col_num) + ".jpg";
    c_path /= c_file;           // adds cfile to c_path

    unsigned char colR[arraysize];
    unsigned char colG[arraysize];
    unsigned char colB[arraysize];

    const char* bufp = static_cast<const char*>(cpoint);

    for (int j=0; j<arraysize; j++){
        const char* cind = bufp + 3*j;
        colR[j] = *(cind);
        colG[j] = *(cind+1);
        colB[j] = *(cind+2);
    }

    // in theory should be able to copy directly to an interleaved view, but need appropriate iterator
    //rgb8_planar_view_t colIm = interleaved_view(COL WIDTH, COLHEIGHT, cpoint, COLWIDTH);

    rgb8_planar_view_t colIm = planar_rgb_view(COLWIDTH, COLHEIGHT, colR, colG, colB, COLWIDTH);
    std::string saveLoc = c_path.string();
    boost::gil::jpeg_write_view(saveLoc, colIm, 95);

    std::cout << "Color frame stored" << std::endl;


    calib_col_num++; // make sure this works

}

void saveDepthIm(rs::device* dev, bfs::path d_path)
{

    // frame number - rarely used in the field but useful for calibration
    static int calib_depth_num = 0;

    const void * dpoint = dev->get_frame_data(rs::stream::depth);
    std::string d_file = "DepthSnap_" + std::to_string(calib_depth_num) + ".dat";
    d_path /= d_file;

    int arraysize = DEPTHWIDTH*DEPTHHEIGHT*2; // 2 bytes per pixel

    bfs::ofstream depthfile;
    depthfile.open(d_path, std::ios::out | std::ofstream::binary);

    // disparity files are uint16
    depthfile.write(static_cast<const char*>(dpoint), arraysize);
    depthfile.close();

    std::cout << "Depth frame stored" << std::endl;
    calib_depth_num++;
}

void saveIRIm(rs::device* dev, bfs::path ir_path)
{
    // frame number - rarely used in the field but useful for calibration
    static int calib_IR_num = 0;

    const void * irpoint = dev->get_frame_data(rs::stream::infrared);
    std::string ir_file = "IRSnap_" + std::to_string(calib_IR_num) + ".dat";
    ir_path /= ir_file;

    int arraysize = DEPTHWIDTH*DEPTHHEIGHT; // 1 byte per pixel

    bfs::ofstream irfile;
    irfile.open(ir_path, std::ios::out | std::ofstream::binary);

    // disparity files are uint16
    irfile.write(static_cast<const char*>(irpoint), arraysize);
    irfile.close();

    std::cout << "IR frame stored" << std::endl;
    calib_IR_num++;
}

/* could use saveIm functions for movies as well but this is slightly faster, and
 *  there are some minor functional differences that make this challenging.
 * future: put some code into a common function?*/

void depthFrame(const void* dpoint, bfs::path d_path, int framenum)
{
    std::string d_file = "depth_frame_" + std::to_string(framenum) + ".dat";
    d_path /= d_file;

    int arraysize = DEPTHWIDTH*DEPTHHEIGHT*2; // 2 bytes per pixel

    bfs::ofstream depthfile;
    depthfile.open(d_path, std::ios::out | std::ofstream::binary);

    // disparity files are uint16
    depthfile.write(static_cast<const char*>(dpoint), arraysize);
    depthfile.close();
}

void colorFrame(const void* cpoint, bfs::path c_path, int framenum)
{
    // Below: may be able to reintroduce compression without harming framerate?
    // Delete up to REINSERT if not

    using namespace boost::gil;

    int arraysize = COLWIDTH*COLHEIGHT;
    if (!hresflag) arraysize = COL_SWIDTH*COL_SHEIGHT;

    std::string c_file = "col_frame_" + std::to_string(framenum) + ".jpg";
    c_path /= c_file;           // adds cfile to c_path

    unsigned char colR[arraysize];
    unsigned char colG[arraysize];
    unsigned char colB[arraysize];

    const char* bufp = static_cast<const char*>(cpoint);

    for (int j=0; j<arraysize; j++){
        const char* cind = bufp + 3*j;
        colR[j] = *(cind);
        colG[j] = *(cind+1);
        colB[j] = *(cind+2);
    }


    rgb8_planar_view_t colIm = planar_rgb_view(COLWIDTH, COLHEIGHT, colR, colG, colB, COLWIDTH);
    std::string saveLoc = c_path.string();
    boost::gil::jpeg_write_view(saveLoc, colIm, 95);
    
}

void colCompress()
{
    using namespace boost::gil;

    // crashes, reason unclear
    boost::this_thread::sleep_for( boost::chrono::milliseconds(200) );

    int filenum;
    bchrono::system_clock::time_point compstart = bchrono::system_clock::now();

    FILE* inputFile;

    int comp_wait;

    // filenum may not have yet gone in!
    while ((g_movflag & 0x01) || timestampList.pop(filenum))
    {
        comp_wait = (1/colframerate)*1000;

        while (!(timestampList.pop(filenum)))
        {
            boost::this_thread::sleep_for( boost::chrono::milliseconds(comp_wait) );
            // check in case movie has been ended in this time
            if (!(g_movflag & 0x01)) break;
        }

        unsigned char readBuf[1024*3]; // 1024 bytes per channel
        std::vector<unsigned char> rBuf;
        std::vector<unsigned char> gBuf;
        std::vector<unsigned char> bBuf;

        std::string c_file = "col_frame_" + std::to_string(filenum) + ".dat";
        bfs::path c_path = cpath;
        bfs::path out_path = cpath;

        c_path /= c_file;
        std::string inName = c_path.string();
        out_path /= "col_frame_" + std::to_string(filenum) + ".jpg";

        const char* inputPt = inName.c_str();

        // copy input file into buffer(s)
        bool fileopen = false;
        int retry = 0;

        while (!fileopen && retry<5)
        {
            try{
                // open input file, if available
                inputFile = fopen(inputPt, "rb");
                fileopen = true;
            }
            catch(...){
            }
        }

        if (inputFile){
            size_t len;
            while ( ( len= fread(readBuf, sizeof(char), sizeof(readBuf), inputFile) > 0))
            {
                for (int j=0; j<1024; j++)
                {
                    rBuf.push_back(readBuf[3*j]);
                    gBuf.push_back(readBuf[3*j + 1]);
                    bBuf.push_back(readBuf[3*j + 2]);
                }
            }

            unsigned char* jR = &rBuf[0];
            unsigned char* jG = &gBuf[0];
            unsigned char* jB = &bBuf[0];

            int closeSuccess = fclose(inputFile);

            int rgbwidth = COLWIDTH;
            int rgbheight = COLHEIGHT;
            if (!hresflag){ rgbwidth = COL_SWIDTH; rgbheight = COL_SHEIGHT;}
            rgb8_planar_view_t colIm = planar_rgb_view(rgbwidth, rgbheight, jR, jG, jB, rgbwidth); //(? 2-6MB?)

            std::string saveLoc = out_path.string();

            boost::gil::jpeg_write_view(saveLoc, colIm, 95);

            // Try further delay:
            if (!closeSuccess)
            {
                // returned 0, delete file
                boost::this_thread::sleep_for( boost::chrono::milliseconds(comp_wait) );
                bfs::remove(c_path);
            }
        }
    }

    bchrono::milliseconds compcount = bchrono::duration_cast<bchrono::milliseconds>(bchrono::system_clock::now() - compstart);

    //std::cout << "Compression took " << compcount.count() << "ms " <<std::endl;

}


static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    using std::cout;
    using std::endl;


    // watch out for bit overlaps
    const unsigned char allmov = 0x01;
    const unsigned char colmov = 0x02;
    const unsigned char depmov = 0x04;

    bfs::path c_path = cpath;
    bfs::path d_path = dpath;
    rs::device * dev = static_cast<rs::device *>(glfwGetWindowUserPointer(window));

    // important: DO NOT TAKE SNAPSHOTS IF MOVIE IS RUNNING
    // don't want to mess with the framerate
    
    // for future: pick up any key, then do a switch statement

    if (key == GLFW_KEY_A && action == GLFW_PRESS){
        //Take a snapshot of all frames
        if (!(g_movflag & 0x01))
        {
            boost::thread colsnapshot(saveColIm, dev, c_path), depthsnapshot(saveDepthIm, dev, d_path), irsnapshot(saveIRIm, dev, d_path);
            colsnapshot.detach();
            depthsnapshot.detach();
            irsnapshot.detach();
        }

    }

    if (key == GLFW_KEY_1 && action == GLFW_PRESS)
    {
        if (!(g_movflag & 0x01))
        {
        // Take a colour snapshot
        boost::thread colsnapshot(saveColIm, dev, c_path);
        colsnapshot.detach();
        }
    }

    if (key == GLFW_KEY_2 && action == GLFW_PRESS)
    {
        if (!(g_movflag & 0x01))
        {
        // Take a depth snapshot
        boost::thread depthsnapshot(saveDepthIm, dev, d_path);
        depthsnapshot.detach();
        }
    }

    if (key == GLFW_KEY_3 && action == GLFW_PRESS)
    {
        if (!(g_movflag & 0x01))
        {
        // take an infrared snapshot
        boost::thread irsnapshot(saveIRIm, dev, d_path);
        irsnapshot.detach();
        }
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS)
    {
        // Turn movie flag bit on
        g_movflag |= allmov;

        // set flag when movie is turned on
        if (g_movflag & 0x01)
            loopflag = true;

        // expect 1
        cout << "Movie recording started (all streams) " << int(g_movflag) << endl;

    }

    //if (key == GLFW_KEY_L && action == GLFW_PRESS)
    //{
    //    // reassign colour and depth frame rates
    //    if (!(g_movflag & 0x01))
    //    {
    //        std::cout << "Warning: Framerates reassigned" << std::endl;
    //    }
    //    if (s_movflag)
    //    {
    //        colframerate = col_fps_new;
    //        depthframerate = dep_fps_new;
    //    }
    // }
    
    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        // Toggle colour stream flag
        g_movflag ^= colmov;
        if (g_movflag & colmov)
            cout << "Movie recording started (colour stream)" << int(g_movflag) << endl;
        else
            cout << "Movie recording stopped (colour stream)" << int(g_movflag) << endl;
    }

    if (key == GLFW_KEY_D && action == GLFW_PRESS)
    {
        g_movflag ^= depmov;
        if (g_movflag & depmov)
            cout << "Movie recording started (depth stream)" << int(g_movflag) << endl;
        else
            cout << "Movie recording stopped (depth stream)" << int(g_movflag) << endl;
    }

    // new control: cycle through exposure options (assume start on auto)
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
         if (!(g_movflag & 0x01))
         {

             // increment current exposure value cyclicly? to a point!
             int init_val = 40;
             int auto_on = dev->get_option(rs::option::color_enable_auto_exposure);

             if (auto_on){  dev->set_option(rs::option::color_exposure, init_val); }
             else {

                int newval;
                int cval = dev->get_option(rs::option::color_exposure);

                if (cval < 900)
                {
                    newval = cval + 45;
                    dev->set_option(rs::option::color_exposure, newval);
                }
                else
                {
                    dev->set_option(rs::option::color_enable_auto_exposure, 1);
                    std::cout << "Auto exposure enabled " << std::endl;
                }

             }

             //std::cout << "Auto on? " << autoOn << "   Current exposure  " << cval << std::endl;


         }
    }

    if (key == GLFW_KEY_S && action == GLFW_PRESS)
    {
        int c_sharp = dev->get_option(rs::option::color_sharpness);
        int new_sharp;

        if (c_sharp < 99) {  new_sharp = c_sharp+5;  }
        else {
            new_sharp = 50;
            std::cout << "Sharpness reset to default " << std::endl;
        }

        dev->set_option(rs::option::color_sharpness, new_sharp);
    }

    if (key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        int white_on = dev->get_option(rs::option::color_enable_auto_white_balance);

        if (white_on) {
            dev->set_option(rs::option::color_white_balance, 9000);
        }
        else {
            int coltemp = dev->get_option(rs::option::color_white_balance);
            if (coltemp > 2500){
                    int colnew = coltemp - 500;
                    dev->set_option(rs::option::color_white_balance, colnew);
            }
            else {
            dev->set_option(rs::option::color_enable_auto_white_balance, 1);
            }
        }
    }

    if (key == GLFW_KEY_E && action == GLFW_PRESS)
    {
        // end all movies (turn flag bits off
        g_movflag = 0x00;
        cout << "All movies stopped" <<  int(g_movflag) << endl;
    }

}

int main()
try
{
    int runNum;
    std::cout << "Enter recording number: ";
    std::cin >> runNum;

    hresflag = true;

    std::cout << "Enter movie framerate: ";
    std::cin >> colframerate;

    depthframerate = colframerate;

    // change stack size to handle file compression quickly
    struct rlimit rl;
    const rlim_t kStackSize = 16*1024*1024; // set to 16MB, should be more than enough
    rl.rlim_cur = kStackSize;
    int result = setrlimit(RLIMIT_STACK, &rl);
    std::cout << "Allocated static memory changed to " << kStackSize << std::endl;

    if (result !=0) { std::cout << "Warning: stack size may be insufficient. setrlimit returned " << result << std::endl;}

    // Create a context object. This object owns the handles to all connected realsense devices.
    rs::context ctx;
    printf("There are %d connected RealSense devices.\n", ctx.get_device_count());
    if(ctx.get_device_count() == 0) return EXIT_FAILURE;

    // may need to add another device later on
    rs::device * dev = ctx.get_device(0);
    printf("\nUsing device 0, an %s\n", dev->get_name());
    printf("    Serial number: %s\n", dev->get_serial());
    printf("    Firmware version: %s\n", dev->get_firmware_version());


    if (!hresflag){
        dev->enable_stream(rs::stream::depth, DEPTHWIDTH, DEPTHHEIGHT, rs::format::z16, FRAMERATE);
        dev->enable_stream(rs::stream::color, COL_SWIDTH, COL_SHEIGHT, rs::format::rgb8, FRAMERATE);
    }
    else {
        // Configure all streams to run at VGA resolution at 30 frames per second
        dev->enable_stream(rs::stream::depth, DEPTHWIDTH, DEPTHHEIGHT, rs::format::z16, FRAMERATE);
        dev->enable_stream(rs::stream::color, COLWIDTH, COLHEIGHT, rs::format::rgb8, FRAMERATE);
        dev->enable_stream(rs::stream::infrared, DEPTHWIDTH, DEPTHHEIGHT, rs::format::y8, FRAMERATE);
     }

    dev->start();

    std::cout << "All streams streaming at " << FRAMERATE << " fps" << std::endl;
    std::cout << "Color recording framerate " << colframerate << ", Depth recording framerate " << depthframerate << std::endl;

    // set up file paths for storing images and movies (use or modify CPATH and DPATH basenames)
    boost::gregorian::date today = boost::gregorian::day_clock::local_day();
    std::string col_folder, depth_folder;
    std::string datestring = boost::gregorian::to_iso_string(today);
    col_folder = datestring + "/RGB_" + std::to_string(runNum) + "/";
    depth_folder = datestring + "/D_" + std::to_string(runNum) + "/";
    cpath /= col_folder;
    dpath /= depth_folder;

    std::cout << cpath << std::endl;
    std::cout << dpath << std::endl;

    // make relevant directories
    try {
        boost::filesystem::create_directories(cpath);
        boost::filesystem::create_directories(dpath);
    }
    catch (bfs::filesystem_error &e){
        std::cerr << e.what() << std::endl;
    }

    const int xwin_max = 1250;
    const int ywin_max = 1200;
    int x_win, y_win;

    if (hresflag) {x_win = xwin_max; y_win = ywin_max;}
    else {x_win = COL_SWIDTH; y_win = COL_SHEIGHT*2;}

    // Open a GLFW window to display our output
    glfwInit();

    // set focus policy in the constructor - should be focused by default, so no problem there
    GLFWwindow * win = glfwCreateWindow(x_win, y_win, "Termite Scanner", nullptr, nullptr);
    glfwMakeContextCurrent(win);

    // Set up key controls
    glfwSetKeyCallback(win, key_callback);
    glfwSetWindowUserPointer(win, dev);

    // Initialize frame numbers
    int dnum = 1000000;
    int cnum = 1000000;

    // intervals must be int but if using strange framerates convert to float for division and then cast to int
    int c_incr = 0;
    int d_incr = 0;

    int c_interval = static_cast<int>(1000/colframerate);
    int d_interval = static_cast<int>(1000/depthframerate);

    std::cout << "cast completed" << std::endl;

    // start internal clock
    bchrono::system_clock::time_point start = bchrono::system_clock::now();
    typedef bchrono::milliseconds ms;


    // thread priority to try and manage processor behaviour - not sure if this makes much difference
    /*struct sched_param saveparam, storeparam, depthsaveparam;
    saveparam.sched_priority = 95;
    depthsaveparam.sched_priority = 90;
    storeparam.sched_priority = 85;*/


    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        rs::frame frame;
        bfs::path c_path = cpath;
        bfs::path d_path = dpath;

        ms tickcount = bchrono::duration_cast<ms>(bchrono::system_clock::now() - start);
        int cstamp = tickcount.count();
        int dstamp = tickcount.count();
        if(dev->is_streaming()) dev->wait_for_frames();
        
        const GLvoid* colim = dev->get_frame_data(rs::stream::color);
        const GLvoid* depthim = dev->get_frame_data(rs::stream::depth);
        const GLvoid* irim = dev->get_frame_data(rs::stream::infrared);

        // Always record with 30fps and synced color/depth (for now)

        if (g_movflag & 0x01)
        {

            if (colframerate < FRAMRATE_LIM)
            {
                if ((cstamp-c_incr) >= c_interval)
                {
                    // store colour frame
                    boost::thread colorframe(colorFrame, static_cast<const void*>(colim), c_path, cnum);
                    //pthread_attr_setschedparam(colorframe.native_handle(), &saveparam);
                    colorframe.detach();

                    boost::thread depthframe(depthFrame, static_cast<const void*>(depthim), d_path, dnum);
                    //pthread_attr_setschedparam(depthframe.native_handle(), &depthsaveparam);
                    depthframe.detach();

                    d_incr = dstamp;
                    dnum++;

                    c_incr = cstamp;
                    cnum++;

                }
            }
            // otherwise record every frame
            else {
                
                // store colour frame
                boost::thread colorframe(colorFrame, static_cast<const void*>(colim), c_path, cnum);
                //pthread_attr_setschedparam(colorframe.native_handle(), &saveparam);
                colorframe.detach();

                boost::thread depthframe(depthFrame, static_cast<const void*>(depthim), d_path, dnum);
                //pthread_attr_setschedparam(depthframe.native_handle(), &depthsaveparam);
                depthframe.detach();
                timestampList.push(cstamp);

                // below is for when we are confident about timestamps
                cnum++;
                dnum++;
            }


        }

        else if (g_movflag & 0x04)
        {

            // depth only
            if ((dstamp-d_incr) >= d_interval)
            {
                boost::thread depthframe(depthFrame, static_cast<const void*>(depthim), d_path, dstamp);
                //pthread_attr_setschedparam(depthframe.native_handle(), &depthsaveparam);
                depthframe.detach();

                d_incr = dstamp;
                dnum++;

                // We want to poll for a change to the flag status of s_movflag, but not too often
                // so here is probably a good place
                if (s_movflag){
                    c_interval = 1000/colframerate;
                    d_interval = 1000/depthframerate;
                }
            }
        }

        else {
            // Display color image as RGB triples - for CPU efficiency reasons, streaming is disabled when recording movies
            glClear(GL_COLOR_BUFFER_BIT);

            // uncomment and fix following if/else if using a small screen (eg USB monitor)

            /*if (hresflag) {
              glPixelZoom(0.5, 0.5);
              glRasterPos2f(-1, -0.58);
              glDrawPixels(COLWIDTH, COLHEIGHT, GL_RGB, GL_UNSIGNED_BYTE, colim);
            }
            else { */
            glPixelZoom(0.6,0.6);
            glRasterPos2f(-1, -0.4);
            glDrawPixels(COLWIDTH, COLHEIGHT, GL_RGB, GL_UNSIGNED_BYTE, colim);

            //glDrawPixels(COL_SWIDTH, COL_SHEIGHT, GL_RGB, GL_UNSIGNED_BYTE, colim);


            // Display depth data by linearly mapping depth between 0 and 1-ish to the red channel
            // (can probably use a better display method here)

            // something is up - maybe a glfw problem? seems uniform across all RS programs but ONLY on new NUC.

            glRasterPos2f(-1, -0.9);
            glPixelTransferf(GL_RED_SCALE, 0xFFFF * dev->get_depth_scale() / 0.25f);
            // if-else on depth frame display
            glDrawPixels(DEPTHWIDTH, DEPTHHEIGHT, GL_RED, GL_UNSIGNED_SHORT, depthim);
            glPixelTransferf(GL_RED_SCALE, 1.0f);


            //Display infrared image by mapping IR intensity to visible luminance
            glRasterPos2f(-0.4, -0.9);
            glDrawPixels(640, 480, GL_LUMINANCE, GL_UNSIGNED_BYTE, irim);

            glfwSwapBuffers(win);
        }

    }

    return EXIT_SUCCESS;
}
catch(const rs::error & e)
{
    // Method calls against librealsense objects may throw exceptions of type rs::error
    printf("rs::error was thrown when calling %s(%s):\n", e.get_failed_function().c_str(), e.get_failed_args().c_str());
    printf("    %s\n", e.what());
    return EXIT_FAILURE;
}
