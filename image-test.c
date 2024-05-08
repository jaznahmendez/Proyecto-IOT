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

// Function to render text onto a buffer
void render_text(unsigned char *buffer, int width, int height, const char *text, int x, int y)
{
    int stride = width * 4; // Always use 4 bytes per pixel (ARGB)
    // Ensure stride is aligned to 4 bytes (32 bits)
    stride = (stride + 3) & ~3;

    // Set text color (black)
    unsigned char text_color[4] = {0, 0, 0, 255}; // ARGB format

    // Define font size and position
    int font_size = 5; // Adjust as needed

    // Define a simple font
    const char *font[] = {
        "  XXX   XX    XX   X    XXXX  XXXX  X   X  XXXX",
        "   X    X X  X X  XX   X     X       X X   X   X",
        "   X    X  XX  X  X X   XXX   XXX    X    X   X",
        "   X    X      X  X  X     X     X   X X   X   X",
        "  XXX   X      X  X   X XXXX  XXXX  X   X  XXXX"};

    // Render each character of the text onto the buffer
    for (int i = 0; text[i]; i++)
    {
        int char_index = text[i] - ' ';
        if (char_index >= 0 && char_index < 95) // Assuming ASCII characters
        {
            for (int j = 0; j < font_size; j++)
            {
                for (int k = 0; k < font_size; k++)
                {
                    if (font[j][char_index * font_size + k] == 'X')
                    {
                        // Set pixel color for text
                        buffer[(y + j) * stride + (x + i * (font_size + 1) + k) * 4 + 0] = text_color[0]; // R
                        buffer[(y + j) * stride + (x + i * (font_size + 1) + k) * 4 + 1] = text_color[1]; // G
                        buffer[(y + j) * stride + (x + i * (font_size + 1) + k) * 4 + 2] = text_color[2]; // B
                        buffer[(y + j) * stride + (x + i * (font_size + 1) + k) * 4 + 3] = text_color[3]; // A
                    }
                }
            }
        }
    }
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

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    // Load JPEG image
    FILE *jpg_file = fopen("image.jpg", "rb");
    if (!jpg_file)
    {
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
    int stride = width * 4; // Always use 4 bytes per pixel (ARGB)
    // Ensure stride is aligned to 4 bytes (32 bits)
    stride = (stride + 3) & ~3;

    int size = stride * height;
    unsigned char *raw_image_data = malloc(size);

    // Read scanlines one by one and copy them into raw_image_data buffer
    unsigned char *row_pointer = raw_image_data;
    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, &row_pointer, 1);
        row_pointer += stride;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(jpg_file);

    // Create shared memory pool and buffer
    int fd = syscall(SYS_memfd_create, "buffer", 0);
    ftruncate(fd, size);
    unsigned int *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    // Copy image data to the buffer, converting from RGB to ARGB
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            unsigned char *src_pixel = &raw_image_data[i * stride + j * num_channels];
            unsigned int *dst_pixel = &data[i * width + j];
            *dst_pixel = 0xFF000000 | (src_pixel[0] << 16) | (src_pixel[1] << 8) | src_pixel[2];
        }
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
                                                         0, width, height, stride, WL_SHM_FORMAT_ARGB8888);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    // Render text onto the buffer
    render_text((unsigned char *)data, width, height, "TITLE", 10, 30);

    free(raw_image_data);

    while (1)
    {
        wl_display_dispatch(display);
    }

    wl_display_disconnect(display);

    return 0;
}
