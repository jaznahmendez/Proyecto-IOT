#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <json-c/json.h>
#include <wayland-client.h>
#include <jpeglib.h>
#include <syscall.h>
#include <sys/mman.h>

struct wl_display *display;
struct wl_compositor *compositor;
struct wl_shm *shm;
struct wl_shell *shell;

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

struct wl_registry *registry;

struct wl_registry_listener listener = {
    registry_global_handler,
    registry_global_remove_handler
};


char* base64_encode(const unsigned char *data, size_t input_length) {
    static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(output_length);
    if (encoded_data == NULL) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = base64_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 0 * 6) & 0x3F];
    }

    for (size_t i = 0; i < input_length % 3; i++) {
        encoded_data[output_length - 1 - i] = '=';
    }

    return encoded_data;
}

void display_image(){
    display = wl_display_connect(NULL);
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &listener, NULL);

    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    struct wl_surface *surface = wl_compositor_create_surface(compositor);
    struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(shell, surface);
    wl_shell_surface_set_toplevel(shell_surface);

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
    int num_channels = cinfo.output_components; 

    int stride = width * 4; 
    stride = (stride + 3) & ~3;

    int size = stride * height;
    unsigned char *raw_image_data = malloc(size);

    unsigned char *row_pointer = raw_image_data;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row_pointer, 1);
        row_pointer += stride;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(jpg_file);

    int fd = syscall(SYS_memfd_create, "buffer", 0);
    ftruncate(fd, size);
    unsigned int *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
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

    free(raw_image_data);

    while (1) {
        wl_display_dispatch(display);
    }

    wl_display_disconnect(display);

    return 0;
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, struct json_object *json_obj) {
    json_object *parsed_json = json_tokener_parse(ptr);
    if (!parsed_json) {
        fprintf(stderr, "Error parsing JSON\n");
        return 0;
    }
    json_object_object_add(json_obj, "response", parsed_json);
    return size * nmemb;
}

void api_call(const char *base64_string) {
    CURL *hnd = curl_easy_init();

    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(hnd, CURLOPT_URL, "https://shazam.p.rapidapi.com/songs/v2/detect?timezone=America%2FChicago&locale=en-US");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "content-type: text/plain");
    headers = curl_slist_append(headers, "X-RapidAPI-Key: f6787ff0a0msh2ab7233cbd00cf2p16188ejsncc0c1d880713");
    headers = curl_slist_append(headers, "X-RapidAPI-Host: shazam.p.rapidapi.com");
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, base64_string);

    struct json_object *response_json = json_object_new_object();

    curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, response_json);

    CURLcode ret = curl_easy_perform(hnd);

    curl_easy_cleanup(hnd);
    curl_slist_free_all(headers);

    if (ret != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(ret));
        json_object_put(response_json);
        return;
    }

    struct json_object * metadata;
    struct json_object * track;
    struct json_object * title;
    struct json_object * subtitle;
    struct json_object * images;
    struct json_object * coverart;
    metadata = json_object_object_get(response_json, "response");
    track = json_object_object_get(metadata, "track");
    title = json_object_object_get(track, "title");
    subtitle = json_object_object_get(track, "subtitle");
    images = json_object_object_get(track, "images");
    coverart = json_object_object_get(images, "coverart");

    printf("JSON CREATED\n");
    //printf("Metadata JSON:\n%s\n", json_object_to_json_string_ext(metadata, JSON_C_TO_STRING_PRETTY));
    printf("Title: %s\n", json_object_get_string(title));
    printf("Subtitle: %s\n", json_object_get_string(subtitle));
    printf("Coverart: %s\n", json_object_get_string(coverart));

    // Print the parsed JSON object
    //printf("Response JSON:\n%s\n", json_object_to_json_string_ext(response_json, JSON_C_TO_STRING_PRETTY));

    json_object_put(response_json);
    char buff[2048];
    strcpy(buff, "wget ");
    strcat(buff, json_object_get_string(coverart));

    system(buff);
    system("mv 400x400cc.jpg image.jpg");
    display_image();
    return;
}


int main() {
    FILE *file = fopen("salmini.raw", "rb");
    if (file == NULL) {
        perror("Error opening file");
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    unsigned char *data = (unsigned char*)malloc(file_size);
    if (data == NULL) {
        perror("Error allocating memory");
        fclose(file);
        return 1;
    }

    fread(data, 1, file_size, file);
    fclose(file);

    char *base64_string = base64_encode(data, file_size);
    if (base64_string == NULL) {
        perror("Error encoding base64");
        free(data);
        return 1;
    }

    api_call(base64_string);
   
    return 0;
}
