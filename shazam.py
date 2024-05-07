import http.client
import base64

def read_file(input_raw_file):
    with open(input_raw_file, "rb") as f:
        return f.read()

def convert_audio_to_base64(audio_file):
    audio_bytes = read_file(audio_file)
    audio_base64 = base64.b64encode(audio_bytes).decode('utf-8')
    return audio_base64

# Example usage
if __name__ == "__main__":
    conn = http.client.HTTPSConnection("shazam.p.rapidapi.com")
    input_raw_file = "salmini.raw"
    base64_str = convert_audio_to_base64(input_raw_file)
    #with open("salmini.txt", "w", encoding="utf-8") as out:
     #   out.write(base64_str)

    payload = base64_str

    headers = {
        'content-type': "text/plain",
        'X-RapidAPI-Key': "1bdffe5b0amshededa1e5ec38cf9p1ba83djsn29c9a4a9dcdd",
        'X-RapidAPI-Host': "shazam.p.rapidapi.com"
    }

    conn.request("POST", "/songs/detect", payload, headers)

    res = conn.getresponse()
    data = res.read()

    print(data.decode("utf-8"))


