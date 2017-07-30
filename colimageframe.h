/* colimageframe.h
 *
 * Description:
 *   header file for colImageFrame class
 *   Can be used to stream compressed RGB data to a JPEG file
 *   Overload methods take either a pointer to the streaming device
 *   itself, or a pointer to a stored RGB buffer.
 *
 * Functions:
 *   col_size_calc - calculates needed buffer size for rgb conversion
 *   save_col_frame - takes a frame from a RealSense library-compatible color image stream OR
 *   a pointer to such a frame buffer, saves to file
 *
 * Input:
 *   device or buffer pointer
 *   path to save directory
 *   filename or enumerative
 *
 * Output:
 *   none
 *
 * Requirements:
 *   librealsense
 *   boost/filesystem
 *   boost/gil
 *   boost/gil/extension
 *   fstream
 *
 * Thread safe? YES
 *
 * Extendable? YES
 */


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
