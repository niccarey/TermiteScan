# TermiteScan
RealSense C application beta for termite scanning and tracking

This software requires librealsense to run. If you don't have librealsense installed, you can get it here: https://github.com/IntelRealSense/librealsense

For implementation reasons, recording (not displaying) framerates above 28fps will default to recording every frame. 
If you wish to specify framerates higher than this, change the value of global variable FRAMERATE_LIM 
(note that most hardware cannot handle storing hi-res colour images at framerates above 30fps - check your processor speed and memory availability before attempting this).
