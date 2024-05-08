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


struct wl_display *display;
struct wl_compositor *compositor;
struct wl_surface *surface;

void registry_global_handler(void *data, struct wl_registry *registry, 
                             uint32_t id, const char *interface, uint32_t version) {
    if (strcmp(interface, "wl_compositor") == 0) {
        compositor = wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
}


struct wl_registry_listener registry_listener = {
    registry_global_handler
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
   

    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_dispatch(display);
    wl_display_roundtrip(display);

    if (!compositor) {
        fprintf(stderr, "Failed to get compositor\n");
        return 1;
    }

    surface = wl_compositor_create_surface(compositor);
    if (!surface) {
        fprintf(stderr, "Failed to create surface\n");
        return 1;
    }

    const char *text = "Hello, Wayland!";
    printf("Printing text: %s\n", text);

    wl_display_roundtrip(display);

    wl_display_disconnect(display);
    



    // Clean up
    json_object_put(response_json);


    return 0;
}
