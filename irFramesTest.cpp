// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2019 Intel Corporation. All Rights Reserved.

#include <librealsense2/rs.hpp> // Include RealSense Cross Platform API
#include <librealsense2/rs_advanced_mode.hpp>

#include <iostream>     // for cout
#include <cstdio>
#include <string>

#include <GLFW/glfw3.h>


#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/bind.hpp>
#include <boost/iterator/zip_iterator.hpp>

#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/chrono/chrono.hpp>

#include "irimageframe.h"
#include "colimageframe.h"
#include "depthimageframe.h"

#define DEPTHWIDTH 1280
#define DEPTHHEIGHT 720
#define COLWIDTH 1280
#define COLHEIGHT 720


namespace bfs = boost::filesystem;
namespace bchrono = boost::chrono;
namespace bgreg = boost::gregorian;

// initialise container for distance values: tuple stack of <frame, distance>
typedef std::vector< std::tuple<int, float> >  pd_array;

unsigned char g_movflag = 0x00;
bool g_alignflag = false;

bfs::path cpath{"../../IRFrameStore/"};
bfs::path dpath{"../../IRFrameStore/"};


static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // User input handling: key press functionality is enabled while GLFW window is open
    using std::cout;
    using std::endl;

    static int calib_num = 0;
    static bool emitter_toggle = true;

    bfs::path c_path = cpath;
    bfs::path d_path = dpath;

    const unsigned char allmov = 0x01;

    rs2::pipeline * pipe_select = static_cast<rs2::pipeline *>(glfwGetWindowUserPointer(window));
    rs2::pipeline_profile current_profile = pipe_select->get_active_profile();
    //rs2::frameset frames;

    // important: DO NOT TAKE SNAPSHOTS IF MOVIE IS RUNNING - messes with framerate

    switch(key) {

    case GLFW_KEY_A: // all frame snapshot
        if ((action == GLFW_PRESS) )
        {
            std::string ir_file_left = "IRLeftSnap_" + std::to_string(calib_num) + ".dat";
            std::string ir_file_right = "IRRightSnap_" + std::to_string(calib_num) + ".dat";
            std::string d_file = "DepthSnap_" + std::to_string(calib_num) + ".dat";
            std::string c_file = "ColSnap_" + std::to_string(calib_num) + ".jpg";

            // set up frame object to put storing frames in
            irImageFrame* irfilesave1 = new irImageFrame(DEPTHWIDTH, DEPTHHEIGHT);
            irImageFrame* irfilesave2 = new irImageFrame(DEPTHWIDTH, DEPTHHEIGHT);

            colImageFrame* cfilesave = new colImageFrame(COLWIDTH, COLHEIGHT);
            depthImageFrame* dfilesave = new depthImageFrame(DEPTHWIDTH, DEPTHHEIGHT);

            typedef void (colImageFrame::*cs_fn)(const void*, bfs::path, std::string);
            typedef void (depthImageFrame::*ds_fn)(const void*, bfs::path, std::string);
            typedef void (irImageFrame::*ir_fn)(const void*, bfs::path, std::string);

            // get current frameset from pipeline
            rs2::frameset mono_frames = pipe_select->wait_for_frames(5000);

            rs2::frame irframe1 = mono_frames.get_infrared_frame(1);
            rs2::frame irframe2 = mono_frames.get_infrared_frame(2);
            rs2::frame depthframe = mono_frames.get_depth_frame();
            rs2::frame colframe = mono_frames.get_color_frame();


            std::cout << "IRpointcheck: " << irframe1.get_data() << std::endl;
            try{
                boost::thread colsnapshot(boost::bind((cs_fn)&colImageFrame::save_col_frame, cfilesave, colframe.get_data(), c_path, c_file));
                colsnapshot.detach();

                boost::thread depthsnapshot(boost::bind((ds_fn)&depthImageFrame::save_d_frame, dfilesave, depthframe.get_data(), d_path, d_file));
                depthsnapshot.detach();

                boost::thread irsnapshot1(boost::bind((ir_fn)&irImageFrame::save_ir_frame, irfilesave1, irframe1.get_data(), d_path, ir_file_left));
                irsnapshot1.detach();

                boost::thread irsnapshot2(boost::bind((ir_fn)&irImageFrame::save_ir_frame, irfilesave2, irframe2.get_data(), d_path, ir_file_right));
                irsnapshot2.detach();

            }

            catch(const rs2::error &e){
                std::cerr <<"Error " << e.get_failed_function() << "(" << e.get_failed_args() <<"):\n    " << e.what() << std::endl;
                break;
            }

            calib_num++;

        }
        break;

    case GLFW_KEY_I:
        if (action == GLFW_PRESS)
        {
            float emitter_value;

            rs2::device dev = current_profile.get_device();
            auto depth_sensor = dev.first<rs2::depth_sensor>();
            if (emitter_toggle){
                emitter_value = 0.0f;
            } else{
                emitter_value = 1.0f;
            }
            emitter_toggle = !emitter_toggle;

            if (depth_sensor.supports(RS2_OPTION_EMITTER_ENABLED)) {
                depth_sensor.set_option(RS2_OPTION_EMITTER_ENABLED, emitter_value);
            }

        }
        break;

    case GLFW_KEY_M:    // start synchronized movie recording
        if (action == GLFW_PRESS){ g_movflag |= allmov;
            cout << "Movie recording started (all streams) " << endl; }
        break;


    case GLFW_KEY_E: // end all movies (turn flag bits off
        if (action == GLFW_PRESS){  g_movflag = 0x00;
            cout << "All movies stopped"  << endl;  }
        break;

    case GLFW_KEY_T:
        if ((action == GLFW_PRESS) ) {
            g_alignflag = !g_alignflag;
        cout << "Frame alignment changed " << endl; }

        break;


    }
}


int main() try
{

    // default values
    float colframerate = 30;
    float depthframerate = 30;

    // Create a Pipeline - this serves as a top-level API for streaming and processing frames
    rs2::pipeline pipe;
    rs2::config cfg;
    rs2::error* e = 0;

    cfg.enable_stream(RS2_STREAM_INFRARED, 1, DEPTHWIDTH, DEPTHHEIGHT, RS2_FORMAT_Y8, 30);
    cfg.enable_stream(RS2_STREAM_INFRARED,2, DEPTHWIDTH, DEPTHHEIGHT, RS2_FORMAT_Y8, 30);
    cfg.enable_stream(RS2_STREAM_COLOR, COLWIDTH, COLHEIGHT, RS2_FORMAT_RGB8, 30);
    cfg.enable_stream(RS2_STREAM_DEPTH, COLWIDTH, COLHEIGHT, RS2_FORMAT_Z16, 30);

    rs2::pipeline_profile selection = pipe.start(cfg);
    rs2::device dev =selection.get_device();
    
    rs2::align align_to_color(RS2_STREAM_COLOR);
    rs2::colorizer c;
    float alpha = 0.5f;
    
    if (dev.is<rs400::advanced_mode>())
    {
        auto advanced_mode_dev = dev.as<rs400::advanced_mode>();
        if (!advanced_mode_dev.is_enabled())
        {
            advanced_mode_dev.toggle_advanced_mode(true);
        }

        STDepthTableControl depth_table = advanced_mode_dev.get_depth_table();
        int disp_shift_val = 250;
        uint32_t depth_units = 100;
        depth_table.disparityShift = disp_shift_val;
        depth_table.depthUnits = depth_units;
        advanced_mode_dev.set_depth_table(depth_table);
    }


    // Configure and start the pipeline
    int x_win = 1800;
    int y_win = 1200;

    // Create files, folders

    typedef bchrono::milliseconds ms;

    std::string lineIn;
    int runNum = 0;

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

    // frame rate interval handling
    int c_incr = 0;
    int c_interval = static_cast<int>(1000/colframerate);

    depthframerate = colframerate;
    // Initialize frame numbers
    int dnum = 1000000;
    int cnum = 1000000;

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
    try {
        boost::filesystem::create_directories(cpath);
        boost::filesystem::create_directories(dpath);
    }
    catch (bfs::filesystem_error &e){
        std::cerr << e.what() << std::endl;
    }



    glfwInit();

    GLFWwindow * win = glfwCreateWindow(x_win, y_win, "D415Imager", nullptr, nullptr);
    if (!win){
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwMakeContextCurrent(win);

    // Set up key controls
    glfwSetKeyCallback(win, key_callback);
    glfwSetWindowUserPointer(win, &pipe); // window pointer used to pass pointer to pipeline

    rs2::frame irframe1, irframe2;
    int pix_x_list[] = {603, 606, 609, 612, 615, 618, 621, 624, 627, 630,};
    int pix_y_list[] = {363, 366, 369, 372, 375, 378, 381, 384, 387, 390,};

    pd_array pixel_distance;
    int framedepthcount = 1;

    
    bchrono::system_clock::time_point start = bchrono::system_clock::now();

    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        bfs::path c_path = cpath;
        bfs::path d_path = dpath;

        ms tickcount = bchrono::duration_cast<ms>(bchrono::system_clock::now() - start);
        int cstamp = tickcount.count();


        // Block program until frames arrive
        rs2::frameset frame_data = pipe.wait_for_frames();

        if (g_alignflag & (framedepthcount<5000))
        {
            frame_data = align_to_color.process(frame_data);
            // to-do: spin off thread? need to preallocate space to make pd_array thread-safe
            rs2::depth_frame dpframe = frame_data.get_depth_frame();

            for (int pxcount=0; pxcount < 10; pxcount++){
                for (int pycount = 0; pycount < 10; pycount++){
                    float distproj_store = dpframe.get_distance(pix_x_list[pxcount], pix_y_list[pycount]);
                    pixel_distance.push_back(std::make_tuple(framedepthcount, distproj_store));
                }
            }
            framedepthcount++;

        }

        // Try to get frames from both infrared streams
        irframe1 = frame_data.get_infrared_frame(1);
        irframe2 = frame_data.get_infrared_frame(2);

        rs2::depth_frame depthframe = frame_data.get_depth_frame();
        rs2::frame colframe = frame_data.get_color_frame();
        

        if (g_movflag & 0x01)
        {
            colImageFrame cfilesave(COLWIDTH, COLHEIGHT);
            typedef void (colImageFrame::*cfn)(const void*, bfs::path, int);

            depthImageFrame dfilesave(DEPTHWIDTH, DEPTHHEIGHT);
            typedef void (depthImageFrame::*dfn)(const void*, bfs::path, int);


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

            glClear(GL_COLOR_BUFFER_BIT);
            glPixelZoom(0.5, 0.5);
            glRasterPos2f(-1,0);
            glDrawPixels(DEPTHWIDTH, DEPTHHEIGHT, GL_LUMINANCE, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(irframe1.get_data()));

            glRasterPos2f(-0.1, 0);
            glDrawPixels(DEPTHWIDTH,DEPTHHEIGHT, GL_LUMINANCE, GL_UNSIGNED_BYTE, static_cast<const GLvoid*>(irframe2.get_data()));

            glRasterPos2f(-1, -0.8);
            glDrawPixels(COLWIDTH, COLHEIGHT, GL_RGB, GL_UNSIGNED_BYTE,static_cast<const GLvoid*>(colframe.get_data()));

            glRasterPos2f(-0.1, -0.8);
            glDrawPixels(DEPTHWIDTH,DEPTHHEIGHT, GL_LUMINANCE, GL_UNSIGNED_SHORT, static_cast<const GLvoid*>(depthframe.get_data()));


        glfwSwapBuffers(win);

    }
    // quick hack to write data to file at end of program
    if (framedepthcount>1){
        std::cout << "Saving aligned distance data ..." << std::endl;

        bfs::path recPath{"../../IRFrameStore/"};
        std::string distRun = datestring + "_" + std::to_string(runNum) + ".csv";
        recPath /= distRun;
        bfs::ofstream recFile;

        recFile.open(recPath);
        while (!pixel_distance.empty())
        {
            std::tuple<int,float> checkTup = pixel_distance.back();
            pixel_distance.pop_back();
            recFile << std::get<0>(checkTup) << ',' << std::get<1>(checkTup) <<std::endl;
        }
        recFile.close();

        
    }

    return EXIT_SUCCESS;
}
catch (const rs2::error & e)
{
    std::cerr << "RealSense error calling " << e.get_failed_function() << "(" << e.get_failed_args() << "):\n    " << e.what() << std::endl;
    return EXIT_FAILURE;
}
catch (const std::exception& e)
{
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
