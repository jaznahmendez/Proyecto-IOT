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
    input_raw_file = "Proyecto\\salmini.raw"
    base64_str = convert_audio_to_base64(input_raw_file)
    with open("salmini.txt", "w", encoding="utf-8") as out:
        out.write(base64_str)
