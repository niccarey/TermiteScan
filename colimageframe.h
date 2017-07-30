#ifndef COLIMAGEFRAME_H
#define COLIMAGEFRAME_H

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/gil/gil_all.hpp>
#include <boost/chrono/chrono.hpp>
#ifndef BOOST_GIL_NO_IO
#include <boost/gil/extension/io/jpeg_io.hpp>
#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/io/dynamic_io.hpp>
#endif

#include <fstream>

// Include the librealsense C++ header file
#include <librealsense/rs.hpp>

class colImageFrame
{
    int width;
    int height;

public:
    colImageFrame(int c_width,int c_height);
    int col_size_calc();
    void save_col_frame(rs::device* dev, boost::filesystem::path c_path, std::string c_file);
    void save_col_frame(const void* cpoint, boost::filesystem::path c_path, int framenum);

};



#endif // COLIMAGEFRAME_H
