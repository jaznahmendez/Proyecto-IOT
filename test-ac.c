#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <json-c/json.h>

#define API_URL "https://api.acoustid.org/v2/lookup"
#define API_KEY "your_api_key_here"

// Callback function to write received data
size_t write_callback(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

// Callback function to receive response
size_t response_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    json_object *json = json_tokener_parse(ptr);
    if (!json) {
        fprintf(stderr, "Error parsing JSON\n");
        return 0;
    }

    // Process JSON object here

    json_object_put(json);

    return nmemb;
}

int main() {
    CURL *curl;
    CURLcode res;
    FILE *fileptr;
    long filelen;
    char *buffer;

    // Open the .wav file
    fileptr = fopen("audio.wav", "rb");
    if (!fileptr) {
        fprintf(stderr, "Error opening file\n");
        return 1;
    }

    // Get the file length
    fseek(fileptr, 0, SEEK_END);
    filelen = ftell(fileptr);
    rewind(fileptr);

    // Allocate memory to contain the whole file
    buffer = (char *)malloc((filelen + 1) * sizeof(char));
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(fileptr);
        return 1;
    }

    // Read the file into the buffer
    if (fread(buffer, sizeof(char), filelen, fileptr) != filelen) {
        fprintf(stderr, "Error reading file\n");
        fclose(fileptr);
        free(buffer);
        return 1;
    }

    // Initialize curl
    curl = curl_easy_init();
    if (curl) {
        // Set the URL
        curl_easy_setopt(curl, CURLOPT_URL, API_URL);

        // Set the API key as a parameter
        char url[1024];
        sprintf(url, "%s?client=%s&format=json", API_URL, API_KEY);
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // Set the POST data
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)filelen);

        // Set response callback
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, response_callback);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            printf("File uploaded successfully\n");
        }

        // Cleanup
        curl_easy_cleanup(curl);
    }

    // Free allocated memory
    free(buffer);
    fclose(fileptr);

    return 0;
}
