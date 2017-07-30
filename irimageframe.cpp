#include "irimageframe.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fstream>

// Include the librealsense C++ header file
#include <librealsense/rs.hpp>

irImageFrame::irImageFrame(int i_width, int i_height)
{
        width = i_width;
        height = i_height;
}


int irImageFrame::ir_size_calc() { return (width*height);}

void irImageFrame::save_ir_frame(rs::device* dev, boost::filesystem::path ir_path, std::string ir_file)
    {
        namespace bfs = boost::filesystem;

        const void * irpoint = dev->get_frame_data(rs::stream::infrared);
        ir_path /= ir_file;

        int arraysize = this->ir_size_calc();

        bfs::ofstream irfile;
        irfile.open(ir_path, std::ios::out | std::ofstream::binary);

        // disparity files are uint16
        irfile.write(static_cast<const char*>(irpoint), arraysize);
        irfile.close();

    }

