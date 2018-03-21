#include "depthimageframe.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fstream>
#include <iostream>

// Include the librealsense C++ header file

#include <librealsense2/rs.hpp>

depthImageFrame::depthImageFrame(int d_width, int d_height)
{
    width = d_width;
    height = d_height;
}

int depthImageFrame::array_size_calc() { return (width*height*2);}

void depthImageFrame::save_d_frame(const void* dpoint, boost::filesystem::path d_path, std::string d_file)
{
       // use this for single, unsynchronised frame-grabbing
        namespace bfs = boost::filesystem;

        //rs2::frame depthframe = frames->first(RS2_STREAM_DEPTH);

        //const void * dpoint = depthframe.get_data(); //dev->get_frame_data(rs::stream::depth);
        d_path /= d_file;

        int arraysize = this->array_size_calc();

        if (arraysize < 614400){
            std::cout << "Error: Possible corruption during save, frame size " << arraysize << std::endl;
        }

        // Occasionally array is not completely written to file (??) but arraysize is returning fine
        // so is not an issue with memory?
        bfs::ofstream depthfile;
        depthfile.open(d_path, std::ios::out | std::ofstream::binary);

        // disparity files are uint16
        depthfile.write(static_cast<const char*>(dpoint), arraysize);
        depthfile.close();


}


void depthImageFrame::save_d_frame(const void* d_point, boost::filesystem::path d_path, int framenum)
{
    // Overloaded: Use this when already have colour frame stored in memory

    namespace bfs = boost::filesystem;

    std::string d_file = "depth_frame_" + std::to_string(framenum) + ".dat";
    d_path /= d_file;

    int arraysize = this->array_size_calc();

    bfs::ofstream depthfile;
    depthfile.open(d_path, std::ios::out | std::ofstream::binary);

    // disparity files are uint16
    depthfile.write(static_cast<const char*>(d_point), arraysize);
    depthfile.close();
}
