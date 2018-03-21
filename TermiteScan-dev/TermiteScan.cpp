/* Program to view and record multiple RealSense streams for imaging purposes.
 *
 * User input: save folder extension (expects int), framerate (expects int <=30)
 * Compatable with Ubuntu 14.04 and 16.10
 * This package will only compile and run with an up-to-date librealsense package and uvcvideo kernel.
 * UPDATE: Reconfiguring dev to run
 * See Readme.md for details
 */


// include standard libraries
#include <iostream>
#include <cstdio>
#include <string>
#include <sys/resource.h>
#include <map>
#include <atomic>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/bind.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/chrono/chrono.hpp>

#include <fstream>
#include <jpeglib.h>
#include <jpegint.h>
// #include "concurrency.hpp"

// Include the librealsense header file
#include <librealsense2/rs.hpp>
#include <librealsense2/rs_advanced_mode.hpp>

// include GLFW for display
#include <GLFW/glfw3.h>

// Local files/headers
#include "depthimageframe.h"
#include "irimageframe.h"
#include "colimageframe.h"


// CONSTANTS
#define COLWIDTH 1920
#define COLHEIGHT 1080
#define DEPTHWIDTH 640
#define DEPTHHEIGHT 480
#define FRAMERATE 30

// SET UP CONSTANTS FOR IMAGE CROPPING


namespace bfs = boost::filesystem;
namespace bchrono = boost::chrono;
namespace bgreg = boost::gregorian;

// global vars
unsigned char g_movflag = 0x00;

bfs::path cpath{"../../TermiteRecord/"};
bfs::path dpath{"../../TermiteRecord/"};

// compression queue: not needed on hardware over 3.1GHz
// boost::lockfree::spsc_queue<int, boost::lockfree::capacity<90000> > timestampList;


static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    /* User input handling: key press functionality is enabled while GLFW window is open */
    using std::cout;
    using std::endl;

    static int calib_num = 0;
    // probably redundant
    static int calib_depth_num = 0;
    static int calib_IR_num = 0;
    static int calib_col_num = 0;

    const unsigned char allmov = 0x01;
    const unsigned char colmov = 0x02;
    const unsigned char depmov = 0x04;

    bfs::path c_path = cpath;
    bfs::path d_path = dpath;

    /* previously: Used the device pointer which let us get both options AND images
     * device pointer is now independent from stream pointer. What can we hold in window pointer
     * to obtain same result? --> try pipe */

    rs2::pipeline * pipe_select = static_cast<rs2::pipeline *>(glfwGetWindowUserPointer(window));
    rs2::pipeline_profile current_profile = pipe_select->get_active_profile();
    //rs2::frameset frames;

    // important: DO NOT TAKE SNAPSHOTS IF MOVIE IS RUNNING - messes with framerate
    // TOTALLY INEXPLICABLE BUG: When using librealsense2, the overloaded method with a filename input (as opposed to number)
    // overwrites some of the constants in memory. Temporary fix: have reverted to integer input where necessary. May
    // manage to fix later.

    switch(key) {
    case GLFW_KEY_A: // all frame snapshot
        if ((action == GLFW_PRESS) && (!(g_movflag & 0x01)))
        {
            std::string d_file = "DepthSnap_" + std::to_string(calib_num) + ".dat";
            std::string ir_file = "IRSnap_" + std::to_string(calib_num) + ".dat";
            std::string c_file = "ColSnap_" + std::to_string(calib_num) + ".jpg";

            // set up frame object to put storing frames in
            colImageFrame* cfilesave = new colImageFrame(COLWIDTH, COLHEIGHT);
            depthImageFrame* dfilesave = new depthImageFrame(DEPTHWIDTH, DEPTHHEIGHT);
            irImageFrame* irfilesave = new irImageFrame(DEPTHWIDTH, DEPTHHEIGHT);

            typedef void (colImageFrame::*cs_fn)(const void*, bfs::path, std::string);
            typedef void (depthImageFrame::*ds_fn)(const void*, bfs::path, std::string);
            typedef void (irImageFrame::*ir_fn)(const void*, bfs::path, std::string);

            //typedef void (colImageFrame::*cs_fn)(const void*, bfs::path, int);
            //typedef void (depthImageFrame::*ds_fn)(const void*, bfs::path, int);
            //typedef void (irImageFrame::*ir_fn)(const void*, bfs::path, int);

            // get current frameset from pipeline
            rs2::frameset mono_frames = pipe_select->wait_for_frames(5000);

            rs2::frame depthframe = mono_frames.get_depth_frame();
            rs2::frame colframe = mono_frames.get_color_frame();
            rs2::frame irframe = mono_frames.get_infrared_frame();

            std::cout << "IRpointcheck: " << irframe.get_data() << std::endl;
            try{
                boost::thread colsnapshot(boost::bind((cs_fn)&colImageFrame::save_col_frame, cfilesave, colframe.get_data(), c_path, c_file));
                colsnapshot.detach();
                boost::thread depthsnapshot(boost::bind((ds_fn)&depthImageFrame::save_d_frame, dfilesave, depthframe.get_data(), d_path, d_file));
                depthsnapshot.detach();
                boost::thread irsnapshot(boost::bind((ir_fn)&irImageFrame::save_ir_frame, irfilesave, irframe.get_data(), d_path, ir_file));
                irsnapshot.detach();

            }

            catch(const rs2::error &e){
                std::cerr <<"Error " << e.get_failed_function() << "(" << e.get_failed_args() <<"):\n    " << e.what() << std::endl;
                break;
            }

            calib_num++;

        }
        break;


    case GLFW_KEY_M:    // start synchronized movie recording
        if (action == GLFW_PRESS) { g_movflag |= allmov;
            cout << "Movie recording started (all streams) " << endl; }
        break;

   /* case GLFW_KEY_P:    // cycle through exposure options

        if ((action == GLFW_PRESS) && (!(g_movflag & 0x01)))
        {
             int init_val = 40;             
             int newval;
             int auto_on = dev->get_option(rs::option::color_enable_auto_exposure);

             if (auto_on){  dev->set_option(rs::option::color_exposure, init_val); }
             else {
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
         }
        break;

    case GLFW_KEY_S: // change sharpness
        if ((action == GLFW_PRESS) && (!(g_movflag & 0x01)))
        {
            int c_sharp = dev->get_option(rs::option::color_sharpness);
            int new_sharpcfg);
    rs2::device dev = selection.get_device();;

            if (c_sharp < 99) {  new_sharp = c_sharp+5;  }
            else {
                new_sharp = 50;
                std::cout << "Sharpness reset to default " << std::endl;
            }

            dev->set_option(rs::option::color_sharpness, new_sharp);
        }
        break;


    case GLFW_KEY_W: // change white balance
        if ((action == GLFW_PRESS) && (!(g_movflag & 0x01)))
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
        break;
        */

    case GLFW_KEY_E: // end all movies (turn flag bits off
        if (action == GLFW_PRESS){  g_movflag = 0x00;
            cout << "All movies stopped"  << endl;  }
        break;

    // default: do nothing
    default:
        if ((action == GLFW_PRESS) && (!(g_movflag & 0x01))){  // random keypress
            cout << "Function keys are M (start movie), E (end movie), A (take snapshots), P (exposure), S (sharpness), W (white balance)" << endl; }

    }
}


int main()

/* streams and records frames from one realsense device.
 * TODO: upgrade to multiple devices, asynchronous.*/

try
{

    // INITIALISE
    int runNum;
    std::string lineIn;

    struct rlimit rl;

    // Declare realsense context
    rs2::context ctx;

    // default values
    float colframerate = 30;
    float depthframerate = 30;

    // TODO: Change this to a dynamic scaling based on screen resolution
    int x_win = 1250;
    int y_win = 1200;

    // Initialize frame numbers
    int dnum = 1000000;
    int cnum = 1000000;

    // frame rate interval handling
    int c_incr = 0;
    int c_interval = static_cast<int>(1000/colframerate);

    // change stack size to handle file compression quickly

    const rlim_t kStackSize = 16*1024*1024; // 16MB
    rl.rlim_cur = kStackSize;
    int result = setrlimit(RLIMIT_STACK, &rl);
    std::cout << "Allocated static memory changed to " << kStackSize << std::endl;
    if (result !=0) { std::cout << "Warning: stack size may be insufficient. setrlimit returned " << result << std::endl;}

    typedef bchrono::milliseconds ms;

    // GET USER INPUT

    while(  (std::cout << "Please enter recording number: ")
            &&    std::getline(std::cin, lineIn)
            &&    !(std::istringstream{lineIn} >> runNum)    )
            {
                std::cerr << "Invalid input, try again." << std::endl;
            }

    while(  (std::cout << "Please enter recording framerate: ")
            &&    std::getline(std::cin, lineIn)
            &&    !(std::istringstream{lineIn} >> colframerate)
            &&    (colframerate > 30)     )
            {
                std::cerr << "Error: valid framerates are integers up to 30fps " << std::endl;
            }

    // SET UP REALSENSE

    depthframerate = colframerate;

    // Adjust for new librealsense
    auto list = ctx.query_devices(); // Get a snapshot of currently connected devices
    if (list.size() == 0)
        throw std::runtime_error("No device detected. Is it plugged in?");

    printf("There are %d connected RealSense devices.\n", list.size());

    // configure pipeline before start
    rs2::pipeline pipe;
    //Create a configuration for configuring the pipeline with a non default profile
    rs2::config cfg;
    //Add desired streams to configuration
    cfg.enable_stream(RS2_STREAM_DEPTH, DEPTHWIDTH, DEPTHHEIGHT, RS2_FORMAT_Z16, FRAMERATE);
    cfg.enable_stream(RS2_STREAM_COLOR, COLWIDTH, COLHEIGHT, RS2_FORMAT_RGB8, FRAMERATE);
    cfg.enable_stream(RS2_STREAM_INFRARED, DEPTHWIDTH, DEPTHHEIGHT, RS2_FORMAT_Y8, FRAMERATE);

    //Instruct pipeline to start streaming with the requested configuration
    rs2::pipeline_profile selection = pipe.start(cfg);
    rs2::device dev = selection.get_device();
    auto depth_sensor = dev.first<rs2::depth_sensor>();

    // printf("\n Using device 0, an %s\n", dev.get_info())
    // printf("    Serial number: %s\n", dev->get_serial());
    // printf("    Firmware version: %s\n", dev->get_firmware_version());

    // Configure all streams to run at VGA resolution at 30 frames per second
    // OUTDATED: librealsense1 version
    // dev->enable_stream(rs::stream::depth, DEPTHWIDTH, DEPTHHEIGHT, rs::format::z16, FRAMERATE);
    // dev->enable_stream(rs::stream::color, COLWIDTH, COLHEIGHT, rs::format::rgb8, FRAMERATE);
    // dev->enable_stream(rs::stream::infrared, DEPTHWIDTH, DEPTHHEIGHT, rs::format::y8, FRAMERATE);
    // dev->start();

    // USE THIS FOR D435 ONLY
    //if (depth_sensor.supports(RS2_OPTION_DEPTH_UNITS))
    //{
    //    depth_sensor.set_option(RS2_OPTION_DEPTH_UNITS, 0.0001);
    //}

    std::cout << "All streams streaming at " << FRAMERATE << " fps" << std::endl;
    std::cout << "Color recording framerate " << colframerate << ", Depth recording framerate " << depthframerate << std::endl;

    // Create files, folders
    bgreg::date today = bgreg::day_clock::local_day();
    std::string col_folder, depth_folder;

    std::string datestring = bgreg::to_iso_string(today);
    col_folder = datestring + "/RGB_" + std::to_string(runNum) + "/";
    depth_folder = datestring + "/D_" + std::to_string(runNum) + "/";

    cpath /= col_folder;
    dpath /= depth_folder;

    std::cout << cpath << std::endl;
    std::cout << dpath << std::endl;

    // make relevant directories
    // these aren't being created but no error is thrown???
    try {
        boost::filesystem::create_directories(cpath);
        boost::filesystem::create_directories(dpath);
    }
    catch (bfs::filesystem_error &e){
        std::cerr << e.what() << std::endl;
    }

    // Open a GLFW window to display our output
    glfwInit();

    GLFWwindow * win = glfwCreateWindow(x_win, y_win, "Termite Scanner", nullptr, nullptr);
    if (!win){
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(win);

    // Set up key controls
    glfwSetKeyCallback(win, key_callback);
    glfwSetWindowUserPointer(win, &pipe); // window pointer used to pass pointer to pipeline profile

    // if I pass the frameset, I can save data but not manipulate the device. If I pass a profile or device pointer, I can manipulate the device but not the stream.
    // is there anything I can pass which can access the frameset as well as the device? without trying to restart an existing pipeline?
    // pipe will let me poll for frames, and can get active profile. Ok.

    std::cout << "cast completed" << std::endl;
    bchrono::system_clock::time_point start = bchrono::system_clock::now();

    while(!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        bfs::path c_path = cpath;
        bfs::path d_path = dpath;

        ms tickcount = bchrono::duration_cast<ms>(bchrono::system_clock::now() - start);
        int cstamp = tickcount.count();

        rs2::frameset stream_data = pipe.wait_for_frames(3000);
        rs2::frame depthframe = stream_data.get_depth_frame();
        rs2::frame colframe = stream_data.get_color_frame();
        rs2::frame irframe = stream_data.get_infrared_frame();

        // Always record with synced color/depth
        if (g_movflag & 0x01)
        {
            colImageFrame cfilesave(COLWIDTH, COLHEIGHT);
            typedef void (colImageFrame::*cfn)(const void*, bfs::path, int);

            depthImageFrame dfilesave(DEPTHWIDTH, DEPTHHEIGHT);
            typedef void (depthImageFrame::*dfn)(const void*, bfs::path, int);

            if (colframerate < 28)
            {  // to save at lower framerates than streaming rates:

                if ((cstamp-c_incr) >= c_interval)
                {
                    // color frame handling
                    boost::thread colorhandler(boost::bind((cfn)&colImageFrame::save_col_frame, &cfilesave, colframe.get_data(), c_path, cnum));
                    colorhandler.detach();
                    // depth frame handling
                    boost::thread depthhandler(boost::bind((dfn)&depthImageFrame::save_d_frame, &dfilesave, depthframe.get_data(), d_path, dnum));
                    depthhandler.detach();

                    dnum++;
                    cnum++;

                    c_incr = cstamp;
                }
            }
            else { // record every frame
                
                // color frame handling
                boost::thread colorhandler(boost::bind((cfn)&colImageFrame::save_col_frame, &cfilesave,  colframe.get_data(), c_path, cnum));
                colorhandler.detach();
                // depth frame handling
                boost::thread depthhandler(boost::bind((dfn)&depthImageFrame::save_d_frame, &dfilesave, depthframe.get_data(), d_path, dnum));
                depthhandler.detach();

                cnum++;
                dnum++;
            }
        }

        else {
            // if not recording, stream:
            glClear(GL_COLOR_BUFFER_BIT);

            // TODO: dynamic monitor sizing
            glPixelZoom(0.6,0.6);
            glRasterPos2f(-1, -0.4);

            glDrawPixels(COLWIDTH, COLHEIGHT, GL_RGB, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(colframe.get_data()));


            // Display depth data by linearly mapping depth between 0 and 1-ish to the red channel
            glRasterPos2f(-1, -0.9);
            // this looks bad.
            glPixelTransferf(GL_RED_SCALE, 0xFFFF * depth_sensor.get_option(RS2_OPTION_DEPTH_UNITS) / 0.25f);
            glDrawPixels(DEPTHWIDTH, DEPTHHEIGHT, GL_RED, GL_UNSIGNED_SHORT, static_cast<const GLvoid*>(depthframe.get_data()));
            glPixelTransferf(GL_RED_SCALE, 1.0f);

            //Display infrared image by mapping IR intensity to visible luminance
            glRasterPos2f(-0.4, -0.9);
            glDrawPixels(640, 480, GL_LUMINANCE, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(irframe.get_data()));

            glfwSwapBuffers(win);
        }

    }

    return EXIT_SUCCESS;
}

catch(const rs2::error & e)
{
    // Method calls against librealsense objects may throw exceptions of type rs::error
    printf("rs2::error was thrown when calling %s(%s):\n", e.get_failed_function().c_str(), e.get_failed_args().c_str());
    printf("    %s\n", e.what());
    return EXIT_FAILURE;
}

