/* depthimageframe.h
 *
 * Description:
 *   header file for depthImageFrame class
 *   Streams raw depth uint16 data to a .dat file
 *   Overload methods take either a pointer to the streaming device
 *   itself, or a pointer to a stored depth buffer.
 *
 * Functions:
 *   depth_size_calc - calculates needed buffer size for rgb conversion
 *   save_d_frame - takes a frame from a RealSense library-compatible depth image stream OR
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
 *   fstream
 *
 * Thread safe? YES
 *
 * Extendable? YES
 */


#ifndef DEPTHIMAGEFRAME_H
#define DEPTHIMAGEFRAME_H

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/chrono/chrono.hpp>

#include <fstream>

// Include the librealsense C++ header file
#include <librealsense/rs.hpp>

class depthImageFrame {
    int width;
    int height;

  public:
    depthImageFrame (int d_width,int d_height);

    int array_size_calc();

    void save_d_frame(rs::device* dev, boost::filesystem::path d_path, std::string d_file);
    void save_d_frame(const void* dpoint, boost::filesystem::path d_path, int framenum);

};



#endif // DEPTHIMAGEFRAME_H
