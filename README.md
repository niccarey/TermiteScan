# TermiteScan
RealSense C++ application beta for termite scanning and tracking
Tested on: Ubuntu 14.04, Ubuntu 16.10
Hardware requirements: at least 8GB RAM (pref 16), 3.4GHz . Performance may be impacted if hardware doesn't meet these specs. Also note, I have not tried to run this code on a microcontroller.

Libraries (executable):
This software requires an updated version of librealsense to run. If you don't have librealsense installed, you can get it here: https://github.com/IntelRealSense/librealsense
Please ensure you have the appropriate hardware and kernel patch.

Libraries (development):
Besides librealsense, to compile this code you will need a large assortment of the boost standard libraries, including boost/gil and boost/gil/extension. Also requires g++-64.
Although this code was developed on QTCreator it does NOT use any QT libraries. 

Effective make call:
qmake DIR/TermiteScan.pro -r -spec linux-g++-64 CONFIG+=release

# How it works
At the time of development, Intel had only provided the most basic SDK for linux users trying to apply the RealSense to their applications. We required a high-precision RGBD recording that kept a very steady framerate and stored depth and colour synchronously (to a precision of at least 1ms). We found it was simpler to write our own code than to try to adapt an existing function. That said, Intel have demonstrated that they are committed to maintaining the linux developer community, and may render this package obsolete in the near future.

Provided hardware meets specifications and librealsense is correctly installed (follow librealsense install procedure linked above), the executable should run out of the box. It displays and records high resolution RGB (1920x1080) and depth (640x480) streams. RGB output is JPEG at 95% compression, to save space. Streamed depth output is saved as raw unit16 frames. Raw IR frames are available as single snapshot frames, but changing the code to add these to the recorded stream would be relatively trivial.

As of July 2017, recording (not displaying) framerates above 28fps will default to recording every frame (ie 29 fps is not possible). If you wish to specify framerates higher than this, change the value of global variable FRAMERATE_LIM. Excerpting frames less than 29 fps will work as before.
(note that most hardware cannot handle storing hi-res colour images at framerates above 30fps - check your processor speed and memory availability before changing these values).
