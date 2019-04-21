#include "irimageframe.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <fstream>
#include <iostream>

// Include the librealsense C++ header file

#include <librealsense2/rs.hpp>

irImageFrame::irImageFrame(int i_width, int i_height)
{
        width = i_width;
        height = i_height;
}


int irImageFrame::array_size_calc() {return (width*height);}

void irImageFrame::save_ir_frame(const void* irpoint, boost::filesystem::path ir_path, std::string ir_file)
    {
        namespace bfs = boost::filesystem;

        //rs2::frame irframe = frames->first(RS2_STREAM_INFRARED);
        //const void * irpoint = irframe.get_data(); //dev->get_frame_data(rs::stream::infrared);
        ir_path /= ir_file;

        int arraysize = this->array_size_calc();
        if (arraysize < 307200){
            std::cout << "Error: Possible corruption during save, frame size " << arraysize << std::endl;
        }

        bfs::ofstream irfile;
        irfile.open(ir_path, std::ios::out | std::ofstream::binary);

        // disparity files are uint16
        irfile.write(static_cast<const char*>(irpoint), arraysize);
        irfile.close();

    }


void irImageFrame::save_ir_frame(const void* irpoint, boost::filesystem::path ir_path, int framenum)
{
    // Overloaded: Use this when already have colour frame stored in memory

    namespace bfs = boost::filesystem;

    std::string ir_file = "ir_frame_" + std::to_string(framenum) + ".dat";
    ir_path /= ir_file;

    int arraysize = this->array_size_calc();


    bfs::ofstream irfile;
    irfile.open(ir_path, std::ios::out | std::ofstream::binary);

    // disparity files are uint16
    irfile.write(static_cast<const char*>(irpoint), arraysize);
    irfile.close();
}
