# Identificador de Canciones con Shazam y Wayland
Laura González Camacho - ISC 734049,
Jazmín Méndez Santana - ISC 734043,
Naim Towfighian Ortiz - ISC 733447,
Luther Williams Sandria - ISC 735869




Este proyecto es un programa en C que utiliza la API de Shazam para identificar una canción a partir de un archivo de audio en formato RAW. Una vez identificada la canción, muestra la portada del álbum asociada a esa canción en una ventana utilizando Wayland.

## Requisitos

- Biblioteca libcurl
- Biblioteca json-c
- Biblioteca Wayland
- Biblioteca libjpeg

## Compilación

Para compilar el proyecto, se necesita tener instaladas las bibliotecas mencionadas anteriormente y luego ejecuta el Makefile

## Uso

1. Coloca el archivo de audio en formato RAW en la misma carpeta que el ejecutable. (La duración del archivo debe ser de alrededor de 4 segundos)
2. Ejecuta el programa.
3. El programa identificará la canción utilizando la API de Shazam y mostrará la portada del álbum asociada a esa canción utilizando Wayland.

## Notas

- Asegúrate de tener una conexión a Internet activa para que el programa pueda realizar la llamada a la API de Shazam.
- La portada del álbum se descargará y guardará como "image.jpg" en la carpeta donde se ejecuta el programa.

## Flujo general del programa

1. Lectura del archivo .raw
2. Calculo del tamaño del archivo
3. Codigicación del archivo a base64
4. Construcción de la llamada a la API
   - URL
   - Headers
5. Respuesta de la API en formato JSON
6. Lectura de datos del JSON
7. Descarga de la imagen de portada
8. Visualización de la imagen
   - Configuración de Wayland
   - Lectura de la imagen
   - Decodificación de la imagen
   - Envio de datos de la imagen
