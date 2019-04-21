/* irimageframe.h
 *
 * Description:
 *   header file for irImageFrame class
 *   Streams raw ir byte data to a .dat file
 *
 * Functions:
 *   ir_size_calc - calculates needed buffer size for rgb conversion
 *   save_ir_frame - takes a frame from a RealSense library-compatible depth image stream,
 *   saves to file
 *
 * Input:
 *   device pointer
 *   path to save directory
 *   filename
 *
 * Output:
 *   none
 *
 * Requirements:
 *   librealsense
 *   boost/filesystem
 *   fstream
 *
 * Thread safe? YES
 *
 * Extendable? YES
 */

#ifndef IRIMAGEFRAME_H
#define IRIMAGEFRAME_H

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fstream>

// Include the librealsense C++ header file
#include <librealsense2/rs.hpp>


class irImageFrame
{
    int width;
    int height;

public:
    irImageFrame( int i_width,int i_height);

    int array_size_calc();

    void save_ir_frame(const void* irpoint, boost::filesystem::path ir_path, std::string ir_file);
    void save_ir_frame(const void* irpoint, boost::filesystem::path ir_path, int framenum);

};

#endif // IRIMAGEFRAME_H
