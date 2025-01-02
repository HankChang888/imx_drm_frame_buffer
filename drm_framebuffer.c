#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

int main() {
    int fd = open("/dev/dri/card1", O_RDWR);
    if (fd < 0) {
        perror("Failed to open DRM device");
        return -1;
    }

    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) {
        perror("Failed to get DRM resources");
        close(fd);
        return -1;
    }

    drmModeCrtc *crtc = drmModeGetCrtc(fd, resources->crtcs[2]);
    if (!crtc) {
        perror("Failed to get CRTC");
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    /* Auto detection CRTC */   
    /*
    drmModeCrtc *crtc = NULL;
    for (int i = 0; i < resources->count_crtcs; i++) {
         crtc = drmModeGetCrtc(fd, resources->crtcs[i]);
        if (crtc && crtc->buffer_id != 0) {
            printf("Using CRTC %d\n", resources->crtcs[i]);
            break;
        }
        if (crtc) drmModeFreeCrtc(crtc);
    }
    if (!crtc) {
        fprintf(stderr, "No active CRTC found.\n");
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }
    */
    if (crtc->buffer_id == 0) {
        printf("No framebuffer associated with the CRTC.\n");
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    drmModeFB *fb = drmModeGetFB(fd, crtc->buffer_id);
    if (!fb) {
        perror("Failed to get framebuffer");
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    printf("Framebuffer ID: %d, Size: %dx%d, Pitch: %d\n",
           fb->fb_id, fb->width, fb->height, fb->pitch);

    struct drm_mode_map_dumb map_dumb = {0};
    map_dumb.handle = fb->handle;

    if (ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb) < 0) {
        perror("Failed to map dumb buffer");
        drmModeFreeFB(fb);
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    size_t size = fb->pitch * fb->height;
    void *mapped_fb = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, map_dumb.offset);
    if (mapped_fb == MAP_FAILED) {
        perror("Failed to mmap framebuffer");
        drmModeFreeFB(fb);
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    printf("Mapped framebuffer at %p, size: %zu\n", mapped_fb, size);

    int capture_width = 640;
    int capture_height = 480;
    int bytes_per_pixel = 4;
    int src_stride = fb->pitch;
    int dest_stride = capture_width * bytes_per_pixel;

    int x_offset = 100;
    int y_offset = 100;

    if (x_offset + capture_width > fb->width || y_offset + capture_height > fb->height) {
        printf("Offset and capture dimensions exceed framebuffer bounds.\n");
        munmap(mapped_fb, size);
        drmModeFreeFB(fb);
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

    uint8_t *dest_buffer = malloc(capture_height * dest_stride);
    if (!dest_buffer) {
        perror("Failed to allocate memory for cropped buffer");
        munmap(mapped_fb, size);
        drmModeFreeFB(fb);
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }
    
    FILE *gstreamer = popen(
        "gst-launch-1.0 fdsrc ! videoparse width=640 height=480 format=bgra framerate=25/1 ! imxvideoconvert_g2d ! " 
	"videoscale ! video/x-raw,width=640,height=480 ! "
	"videoconvert ! videorate ! video/x-raw,framerate=30/1 ! "
	"waylandsink  window-width=640 window-height=480 sync=false ",
        "w");
    if (!gstreamer) {
        perror("Failed to start GStreamer");
        free(dest_buffer);
        munmap(mapped_fb, size);
        drmModeFreeFB(fb);
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(fd);
        return -1;
    }

