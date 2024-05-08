#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <wayland-client.h>
#include <jpeglib.h>
#include <string.h>
#include <syscall.h>
#include <sys/mman.h>

struct wl_display *display;

/*Get Proxy Objects*/
struct wl_compositor *compositor;
struct wl_shm *shm;
struct wl_shell *shell;

/*Global Handlers*/
void registry_global_handler(void *data, struct wl_registry *registry, 
        uint32_t id, const char *interface, uint32_t version) {

    printf("interface %s, version %d, id: %d\n", interface, version, id);

    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    } else if (strcmp(interface, "wl_shm") == 0) {
        shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    } else if (strcmp(interface, "wl_shell") == 0) {
        shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
}

void registry_global_remove_handler(void *data, struct wl_registry *registry, 
        uint32_t id) {

    printf("Removed %d\n", id);
}

/*Global Singleton Object to notify other objects*/
struct wl_registry *registry;

/*Callbacks*/
struct wl_registry_listener listener = {
    registry_global_handler,
    registry_global_remove_handler
};

int main(int argc, char *argv[]) {

    /*Connect to Server*/
    display = wl_display_connect(NULL);
    
    if(display) {
        printf("Connected to Wayland Server\n");
    } else {
        printf("Error Connecting\n");
        return -1;
    }

    /*Get Global Registry Object*/
    registry = wl_display_get_registry(display);
    /*Add Listeners to get*/
    wl_registry_add_listener(registry, &listener, NULL);

    /*Wait for all objects to be listed*/
    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor && shm && shell) {
        printf("Got all objects\n");
    } else {
        printf("Objects cannot be retrieved\n");
        return -1;
    }

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    // Load JPEG image
    FILE *jpg_file = fopen("image.jpg", "rb");
    if (!jpg_file) {
        printf("Error opening image file\n");
        return -1;
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, jpg_file);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int num_channels = cinfo.output_components; // Number of color channels

    // Calculate stride (bytes per row) based on the width and number of channels
    int stride = width * num_channels;
    // Ensure stride is aligned to 4 bytes (32 bits)
    stride = (stride + 3) & ~3;

    int size = stride * height;
    unsigned char *raw_image_data = malloc(size);

    // Read scanlines one by one and copy them into raw_image_data buffer
    unsigned char *row_pointer = raw_image_data;
    while (cinfo.output_scanline < cinfo.output_height) {
        row_pointer += jpeg_read_scanlines(&cinfo, &row_pointer, 1) * stride;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(jpg_file);

    // Create shared memory pool and buffer
    int fd = syscall(SYS_memfd_create, "buffer", 0);
    ftruncate(fd, size);
    unsigned int *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memcpy(data, raw_image_data, size);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
        0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    free(raw_image_data);

    while (1) {
        wl_display_dispatch(display);
    }

    wl_display_disconnect(display);

    return 0;
}