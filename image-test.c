#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <wayland-client.h>
#include <jpeglib.h>
#include <string.h>
#include <syscall.h>
#include <sys/mman.h>

struct wl_display *display;

/* Get Proxy Objects */
struct wl_compositor *compositor;
struct wl_shm *shm;
struct wl_shell *shell;
struct wl_surface *surface;

/* Global Handlers */
void registry_global_handler(void *data, struct wl_registry *registry,
                             uint32_t id, const char *interface, uint32_t version)
{
    printf("interface %s, version %d, id: %d\n", interface, version, id);

    if (strcmp(interface, "wl_compositor") == 0)
    {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
    else if (strcmp(interface, "wl_shm") == 0)
    {
        shm = wl_registry_bind(registry, id, &wl_shm_interface, 1);
    }
    else if (strcmp(interface, "wl_shell") == 0)
    {
        shell = wl_registry_bind(registry, id, &wl_shell_interface, 1);
    }
}

void registry_global_remove_handler(void *data, struct wl_registry *registry,
                                    uint32_t id)
{
    printf("Removed %d\n", id);
}

/* Global Singleton Object to notify other objects */
struct wl_registry *registry;

/* Callbacks */
struct wl_registry_listener listener = {
    registry_global_handler,
    registry_global_remove_handler};

// Function to load and resize JPEG image
unsigned char *load_and_resize_jpeg(const char *filename, int target_width, int target_height)
{
    FILE *jpg_file = fopen(filename, "rb");
    if (!jpg_file)
    {
        printf("Error opening image file\n");
        return NULL;
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

    // Allocate memory for the resized image
    unsigned char *resized_image_data = malloc(target_width * target_height * num_channels);
    if (!resized_image_data)
    {
        printf("Error allocating memory for resized image\n");
        return NULL;
    }

    // Create scaling information
    struct jpeg_compress_struct dstinfo;
    struct jpeg_error_mgr dsterr;

    dstinfo.err = jpeg_std_error(&dsterr);
    jpeg_create_compress(&dstinfo);
    jpeg_mem_dest(&dstinfo, &resized_image_data, target_width * target_height * num_channels);
    dstinfo.image_width = target_width;
    dstinfo.image_height = target_height;
    dstinfo.input_components = num_channels;
    dstinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&dstinfo);
    jpeg_start_compress(&dstinfo, TRUE);

    // Scale the image
    int row_stride = target_width * num_channels;
    JSAMPROW row_buffer = malloc(row_stride);
    while (dstinfo.next_scanline < dstinfo.image_height)
    {
        // Read the next scanline from the original image
        JSAMPROW src_buffer = resized_image_data + dstinfo.next_scanline * row_stride;
        jpeg_write_scanlines(&dstinfo, &src_buffer, 1);
    }

    // Finish compression
    jpeg_finish_compress(&dstinfo);
    jpeg_destroy_compress(&dstinfo);

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(jpg_file);

    return resized_image_data;
}

int main(int argc, char *argv[])
{

    /* Connect to Server */
    display = wl_display_connect(NULL);

    if (display)
    {
        printf("Connected to Wayland Server\n");
    }
    else
    {
        printf("Error Connecting\n");
        return -1;
    }

    /* Get Global Registry Object */
    registry = wl_display_get_registry(display);
    /* Add Listeners to get */
    wl_registry_add_listener(registry, &listener, NULL);

    /* Wait for all objects to be listed */
    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (compositor && shm && shell)
    {
        printf("Got all objects\n");
    }
    else
    {
        printf("Objects cannot be retrieved\n");
        return -1;
    }

    surface = wl_compositor_create_surface(compositor);
    struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    // Load and resize JPEG image to match screen resolution
    int target_width = 720; // Width of the screen
    int target_height = 1280; // Height of the screen
    unsigned char *resized_image_data = load_and_resize_jpeg("image.jpg", target_width, target_height);
    if (!resized_image_data)
    {
        return -1;
    }

    // Create shared memory pool and buffer for resized image
    int num_channels = 3; // Assuming RGB image
    int stride = target_width * num_channels; // 3 bytes per pixel
    int size = stride * target_height;

    int fd = syscall(SYS_memfd_create, "buffer", 0);
    ftruncate(fd, size);
    unsigned int *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memcpy(data, resized_image_data, size); // Copy image data to buffer

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
                                                          0, target_width, target_height, stride, WL_SHM_FORMAT_XRGB8888);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    free(resized_image_data); // Free memory allocated for resized image data

    while (wl_display_dispatch(display) != -1)
    {
        // Wait for the compositor to close the surface
    }

    wl_display_disconnect(display);

    return 0;
}
