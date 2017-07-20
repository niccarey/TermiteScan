/*******************************************************\
* Termite Scan Linux deploy: Beta version               *
\*******************************************************/

/* First include the librealsense C header file */
#include <../../librealsense-master/include/librealsense/rs.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

/* Also include GLFW to allow for graphical display */
#include <GLFW/glfw3.h>

/* Function calls to librealsense may raise errors of type rs_error */
rs_error * e = 0;
void check_error()
{
    if(e)
    {
        printf("rs_error was raised when calling %s(%s):\n", rs_get_failed_function(e), rs_get_failed_args(e));
        printf("    %s\n", rs_get_error_message(e));
        exit(EXIT_FAILURE);
    }
}

int main()
{
    /* Create a context object. This object owns the handles to all connected realsense devices. */
    rs_context * ctx = rs_create_context(RS_API_VERSION, &e);
    check_error();
    printf("There are %d connected RealSense devices.\n", rs_get_device_count(ctx, &e));
    check_error();
    if(rs_get_device_count(ctx, &e) == 0) return EXIT_FAILURE;

    /* This tutorial will access only a single device, but it is trivial to extend to multiple devices */
    rs_device * dev = rs_get_device(ctx, 0, &e);
    check_error();
    printf("\nUsing device 0, an %s\n", rs_get_device_name(dev, &e));
    check_error();
    printf("    Serial number: %s\n", rs_get_device_serial(dev, &e));
    check_error();
    printf("    Firmware version: %s\n", rs_get_device_firmware_version(dev, &e));
    check_error();

    /* Configure all streams to run at 30 frames per second */
    // does not like DISPARITY16 for some reason. that's fine.
    rs_enable_stream(dev, RS_STREAM_DEPTH, 640, 480, RS_FORMAT_Z16, 30, &e);
    check_error();
    rs_enable_stream(dev, RS_STREAM_COLOR, 1920, 1080, RS_FORMAT_RGB8, 30, &e);
    check_error();
    rs_enable_stream(dev, RS_STREAM_INFRARED, 640, 480, RS_FORMAT_Y8, 30, &e);
    check_error();
    rs_start_device(dev, &e);
    check_error();

    // GTK window can be used for interactivity but not sure how this works with C
    // Look into keyboard control (even just an event handler with text on the GLFW window??

    /* Open a GLFW window to display our output */
    // Implement key controls to record movies or snapshots
    glfwInit();
    GLFWwindow * win = glfwCreateWindow(960, 840, "Dedicated Termite Build Scanner", NULL, NULL);
    glfwMakeContextCurrent(win);
    while(!glfwWindowShouldClose(win))
    {
        /* Wait for new frame data */
        glfwPollEvents();
        rs_wait_for_frames(dev, &e);
        check_error();

        const GLvoid* colLoc = rs_get_frame_data(dev, RS_STREAM_COLOR, &e);
        const uint16_t * depthIm = (const uint16_t *)rs_get_frame_data(dev, RS_STREAM_DEPTH, &e);
        const GLvoid* irLoc = rs_get_frame_data(dev, RS_STREAM_INFRARED, &e);

        // check for flags
        // case/switch statement for which flags are set
        //

        glClear(GL_COLOR_BUFFER_BIT);
        glPixelZoom(0.5, 0.5);

        /* Display color image as RGB triples */
        glRasterPos2f(-1, -0.62);
        glDrawPixels(1920, 1080, GL_RGB, GL_UNSIGNED_BYTE, rs_get_frame_data(dev, RS_STREAM_COLOR, &e));
        check_error();

        /* display depth image by mapping and scaling to a red channel*/
        glRasterPos2f(-1, -0.9);
        glPixelTransferf(GL_RED_SCALE, 0xFFFF * rs_get_device_depth_scale(dev, &e) / 0.8f);
        check_error();
        glDrawPixels(640, 480, GL_RED, GL_UNSIGNED_SHORT, rs_get_frame_data(dev, RS_STREAM_DEPTH, &e));
        check_error();
        glPixelTransferf(GL_RED_SCALE, 1.0f);


        /* Display infrared image by mapping IR intensity to visible luminance */
        glRasterPos2f(-0.4, -0.9);
        glDrawPixels(640, 480, GL_LUMINANCE, GL_UNSIGNED_BYTE, rs_get_frame_data(dev, RS_STREAM_INFRARED, &e));
        check_error();

        glfwSwapBuffers(win);
    }

    return EXIT_SUCCESS;
}
