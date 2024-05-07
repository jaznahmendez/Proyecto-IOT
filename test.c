#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <json-c/json.h>

#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <linux/input.h>
#include <semaphore.h>

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

    //printf("%s\n", base64_string);
    
    CURL *hnd = curl_easy_init();

    curl_easy_setopt(hnd, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(hnd, CURLOPT_URL, "https://shazam.p.rapidapi.com/songs/v2/detect?timezone=America%2FChicago&locale=en-US");

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "content-type: text/plain");
    headers = curl_slist_append(headers, "X-RapidAPI-Key: f6787ff0a0msh2ab7233cbd00cf2p16188ejsncc0c1d880713");
    headers = curl_slist_append(headers, "X-RapidAPI-Host: shazam.p.rapidapi.com");
    curl_easy_setopt(hnd, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, base64_string);
    char response_buffer[10000] = {0};

    curl_easy_setopt(hnd, CURLOPT_WRITEDATA, response_buffer);


    CURLcode ret = curl_easy_perform(hnd);
    struct json_object *parsed_json = json_tokener_parse(response_buffer);
            // Now you can use the parsed_json object to access the JSON data
            
            // Print the parsed JSON
    printf("Parsed JSON:\n%s\n", json_object_to_json_string_ext(parsed_json, JSON_C_TO_STRING_PRETTY));
            
            // Don't forget to free the parsed JSON object when done
    json_object_put(parsed_json);
        


    free(data);
    free(base64_string);
    return 0;
}
