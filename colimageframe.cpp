#include "colimageframe.h"

// standard boost libraries
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/gil/gil_all.hpp>

// image handling libraries
#ifndef BOOST_GIL_NO_IO
#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/io/dynamic_io.hpp>
#endif

#include <fstream>
#include <iostream>

// Include the librealsense C++ header file

#include <librealsense2/rs.hpp>

colImageFrame::colImageFrame(int c_width, int c_height)
{
    width = c_width;
    height = c_height;
}

int colImageFrame::col_size_calc() {return (width*height);}

void colImageFrame::save_col_frame(const void* cpoint, boost::filesystem::path c_path, std::string c_file)
{
    // use this for single, unsynchronised frame-grabbing
    using namespace boost::gil;
    int arraysize = this->col_size_calc();
    // need pointer to frame data ...
    //rs2::frame colframe = frames->get_color_frame();
    //const void * cpoint = colframe.get_data();  //dev->get_frame_data(rs::stream::color);

    c_path /= c_file;                            // adds cfile to c_path

    unsigned char colR[arraysize];
    unsigned char colG[arraysize];
    unsigned char colB[arraysize];

    const char* bufp = static_cast<const char*>(cpoint);


    for (int j=0; j<arraysize; ++j){
        const char* cind = bufp + 3*j;
        colR[j] = *(cind);
        colG[j] = *(cind+1);
        colB[j] = *(cind+2);
    }

    // in theory should be able to copy directly to an interleaved view with appropriate iterator
    rgb8_planar_view_t colIm = planar_rgb_view(width, height, colR, colG, colB, width);
    std::string saveLoc = c_path.string();
    boost::gil::jpeg_write_view(saveLoc, colIm, 95);
    std::cout << "Color frame stored" << std::endl;

}


void colImageFrame::save_col_frame(const void* cpoint, boost::filesystem::path c_path, int framenum)
{
    // Overloaded: Use this when already have colour frame stored in memory

    using namespace boost::gil;

    int arraysize = this->col_size_calc();

    std::string c_file = "col_frame_" + std::to_string(framenum) + ".jpg";
    c_path /= c_file;           // adds cfile to c_path

    unsigned char colR[arraysize];
    unsigned char colG[arraysize];
    unsigned char colB[arraysize];

    const char* bufp = static_cast<const char*>(cpoint);

    for (int j=0; j<arraysize; ++j){
        const char* cind = bufp + 3*j;
        colR[j] = *(cind);
        colG[j] = *(cind+1);
        colB[j] = *(cind+2);
    }

    rgb8_planar_view_t colIm = planar_rgb_view(width, height, colR, colG, colB, width);
    std::string saveLoc = c_path.string();
    boost::gil::jpeg_write_view(saveLoc, colIm, 95);

}
