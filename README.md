# imx_drm_frame_buffer
Capture Desktop Image via DRM
-------------------------------------------------
1. Force Enable HDMI Output
To enable HDMI output forcefully:

    # Turn off the HDMI connection
    echo "off" > /sys/class/drm/card1-HDMI-A-1/status
    # Set the resolution to 1920x1080 at 60Hz
    cvt 1920 1080 60
    # Turn on the HDMI connection
    echo "on" > /sys/class/drm/card1-HDMI-A-1/status

2. Compile the DRM Framebuffer Program
To compile the program using GCC:
	gcc -o drm_framebuffer drm_framebuffer.c $(pkg-config --cflags --libs libdrm)
	
3. Run the Program
Execute the compiled program with superuser privileges:
    sudo ./drm_framebuffer
-------------------------------------------------    
