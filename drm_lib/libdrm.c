#include <fcntl.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

static int drm_fd = -1;
static drmModeRes *resources = NULL;
static drmModeCrtc *crtc = NULL;
static drmModeFB *fb = NULL;
static void *mapped_fb = NULL;
static size_t fb_size = 0;
int bytes_per_pixel = 0;
int src_stride = 0;
int dest_stride = 0;

int drm_initialize(const char *device_path, int assign_crt_index) {
    drm_fd = open(device_path, O_RDWR);
    if (drm_fd < 0) {
        perror("Failed to open DRM device");
        return -1;
    }

    resources = drmModeGetResources(drm_fd);
    if (!resources) {
        perror("Failed to get DRM resources");
        close(drm_fd);
        return -1;
    }

    if (assign_crt_index >= 0) {
        crtc = drmModeGetCrtc(drm_fd, resources->crtcs[assign_crt_index]);
    
        if (!crtc || crtc->buffer_id == 0) {
            fprintf(stderr, "No active CRTC with framebuffer found.\n");
            drmModeFreeResources(resources);
            close(drm_fd);
            return -1;
        }
    }else {
        
	for (int search_crt_index = 0; search_crt_index < resources->count_crtcs; search_crt_index++) {
            crtc = drmModeGetCrtc(drm_fd, resources->crtcs[search_crt_index]);
            if (crtc && crtc->buffer_id != 0) {
                printf("Using CRTC %d\n", resources->crtcs[search_crt_index]);
                break;
            }
            if (crtc) drmModeFreeCrtc(crtc);
        }
        if (!crtc) {
            fprintf(stderr, "No active CRTC found.\n");
            drmModeFreeResources(resources);
            close(drm_fd);
            return -1;
        }

    }

    if (crtc->buffer_id == 0) {
        printf("No framebuffer associated with the CRTC.\n");
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    fb = drmModeGetFB(drm_fd, crtc->buffer_id);
    if (!fb) {
        perror("Failed to get framebuffer");
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    struct drm_mode_map_dumb map_dumb = {0};
    map_dumb.handle = fb->handle;
    if (ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_dumb) < 0) {
        perror("Failed to map dumb buffer");
        drmModeFreeFB(fb);
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    fb_size = fb->pitch * fb->height;
    mapped_fb = mmap(NULL, fb_size, PROT_READ, MAP_SHARED, drm_fd, map_dumb.offset);
    if (mapped_fb == MAP_FAILED) {
        perror("Failed to mmap framebuffer");
        drmModeFreeFB(fb);
        drmModeFreeCrtc(crtc);
        drmModeFreeResources(resources);
        close(drm_fd);
        return -1;
    }

    printf("DRM initialized: %dx%d, Pitch: %d, Framebuffer Size: %zu\n", fb->width, fb->height, fb->pitch, fb_size);
    return 0;  // Success
}

void drm_cleanup() {
    if (mapped_fb) munmap(mapped_fb, fb_size);
    if (fb) drmModeFreeFB(fb);
    if (crtc) drmModeFreeCrtc(crtc);
    if (resources) drmModeFreeResources(resources);
    if (drm_fd >= 0) close(drm_fd);
}

void drm_capture_format(int width, int height, int px_size) {	
    if (width <= 0 || height <= 0 || px_size <= 0) {
        fprintf(stderr, "Invalid capture format dimensions or pixel size.\n");
        return;
    }
    bytes_per_pixel = px_size;
    src_stride = fb->pitch;
    dest_stride = width * bytes_per_pixel;
    printf("Capture format set: Width: %d, Height: %d, Bytes per Pixel: %d\n", width, height, bytes_per_pixel);
}    

int drm_capture_frame(uint8_t *dest_buffer, int x_offset, int y_offset, int width, int height) {
    if (!mapped_fb || !fb) {
        fprintf(stderr, "DRM is not initialized.\n");
        return -1;
    }

    if (x_offset + width > fb->width || y_offset + height > fb->height) {
        fprintf(stderr, "Capture dimensions exceed framebuffer bounds.\n");
        return -1;
    }

    for (int y = 0; y < height; y++) {
        memcpy(dest_buffer + y * dest_stride,
               (uint8_t *)mapped_fb + (y + y_offset) * src_stride + x_offset * bytes_per_pixel,
               dest_stride);
    }
    
    //uint8_t *src = (uint8_t *)mapped_fb + y_offset * src_stride + x_offset * bytes_per_pixel;
    //size_t copy_size = height * dest_stride;

    //memcpy(dest_buffer, src, copy_size);
    //printf("Frame captured: Offset (%d, %d), Dimensions: %dx%d\n", x_offset, y_offset, width, height);
    return 0;  // Success
}

