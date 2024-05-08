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
struct wl_surface *surface;

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

    surface = wl_compositor_create_surface(compositor);
    struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

    // Set background color (solid color)
    uint32_t color = 0xFF0000FF; // ARGB format (Alpha, Red, Green, Blue)

    // Create shared memory pool and buffer for solid color
    int stride = 4; // 4 bytes per pixel for ARGB8888 format
    int width = 1920; // Width of the screen
    int height = 1080; // Height of the screen
    int size = stride * width * height; // Size of the buffer

    int fd = syscall(SYS_memfd_create, "buffer", 0);
    ftruncate(fd, size);
    unsigned int *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(data, color, size); // Fill buffer with the solid color

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool,
        0, width, height, stride * 8, WL_SHM_FORMAT_ARGB8888);

    wl_surface_attach(surface, buffer, 0, 0);
    wl_surface_commit(surface);

    while (wl_display_dispatch(display) != -1) {
        // Wait for the compositor to close the surface
    }

    wl_display_disconnect(display);

    return 0;
}
