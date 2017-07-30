#ifndef IRIMAGEFRAME_H
#define IRIMAGEFRAME_H

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fstream>

// Include the librealsense C++ header file
#include <librealsense/rs.hpp>


class irImageFrame
{
    int width;
    int height;

public:
    irImageFrame( int i_width,int i_height);

    int ir_size_calc();

    void save_ir_frame(rs::device* dev, boost::filesystem::path ir_path, std::string ir_file);

};

#endif // IRIMAGEFRAME_H
