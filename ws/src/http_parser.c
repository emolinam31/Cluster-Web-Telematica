/*
 * =============================================================================
 * WS - http_parser.c - Parser de peticiones HTTP/1.1
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. Parsear la línea de request (Request-Line): "METHOD URI VERSION\r\n"
 *      - Extraer method, uri, version con sscanf o strtok
 *   2. Parsear los headers línea por línea buscando:
 *      - Host: (obligatorio en HTTP/1.1)
 *      - Content-Length: (necesario para POST)
 *   3. Validar que method sea GET, HEAD o POST → si no, retornar -1
 *   4. Validar que version sea HTTP/1.1 → si no, retornar -1
 *   5. El body del POST empieza después de "\r\n\r\n"
 *
 * PASOS A TOMAR:
 *   - Paso 1: Tokenizar primera línea → method, uri, version
 *   - Paso 2: Loop sobre líneas restantes → buscar headers relevantes
 *   - Paso 3: Si es POST, guardar puntero al body (offset desde "\r\n\r\n")
 *   - Paso 4: Retornar 0 si todo OK, -1 si request malformado
 *
 * CLAVES PARA EL ÉXITO:
 *   - Las líneas HTTP terminan en "\r\n" (CRLF), NO solo "\n"
 *   - Los headers son case-insensitive: "Host:" == "host:" == "HOST:"
 *     → usar strcasecmp o convertir a minúsculas
 *   - No confiar en que el request tenga formato perfecto,
 *     si algo falta retornar -1 (→ genera 400 Bad Request)
 * =============================================================================
 */


#include "http_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/*Verifica si el metodo recibido es uno de los 3 que vamos a soportar*/
static int metodo_soportado(const char *method){
    if (strcmp(method, "GET") == 0) return 1;
    if (strcmp(method, "HEAD") == 0) return 1;
    if (strcmp(method, "POST") == 0) return 1;
    return 0;
}

/*Verificacion de version HTTP para verificar que sea unicamente HTTP 1.1*/
static int version_soportada(const char *version){
    if (strcmp(version, "HTTP/1.1") == 0) return 1;
    return 0;
}

int http_parse_request(const char *buffer, http_request_t *req){

    /*Chequear que los parametros esten limpios*/
    if (buffer == NULL || req == NULL){
        return -1;
    }

    /* Limpiar la estructura de salida para que todos los campos queden en 0 */
    memset(req, 0, sizeof(http_request_t));

    /*
    PASO 1: parserar la request line en formato "METHOD SP URI SP VERSION CRLF" 
    Ejemplo: "GET /index.html HTTP/1.1" 
    */

    if (sscanf(buffer,"%7s %2047s %15s", req->method, req->uri, req->version) != 3) {
        return -1;
    }

    /*
    Paso 2: el metodo deber ser GET, HEAD o POST
    */
    
    if(!metodo_soportado(req->method)){
        return -1;
    }

    /*
    Paso 3: La version debe ser HTTP1.1 
    */
    if(!version_soportada(req->version)){
        return -1;
    }

    /*
    Paso 4: Localizar el separador headers/body
    * RFC 2616 seccion 4.1: una linea vacia (CRLF CRLF) marca el fin de los
    * headers y el inicio del body. Si no aparece, los headers terminan
    * donde termine el buffer.
    */

    const char *fin_headers = strstr(buffer, "\r\n\r\n");
    if (fin_headers == NULL) {
        return -1;
    }

    /*
     * Paso 5: Recorrer las lineas de headers buscando los que nos importan.
     * Empezamos despues del primer CRLF, que cierra la Request-Line.
    */

    const char *linea = strstr(buffer, "\r\n");
    if (linea == NULL){
        return -1; /*Si la peticion esta mal*/
    }
    linea += 2;

    while (linea < fin_headers){
        /* Host: valor  -> obligatorio en HTTP/1.1 (RFC 2616 14.23) */
        if (strncasecmp(linea, "Host:", 5) == 0){
            const char *valor = linea + 5;
            while (*valor == ' ' || *valor == '\t') {
                valor++;
            }
            sscanf(valor, "%255[^\r\n]", req->host);
        }
        /* Content-Length: Valor -> necesario para leer el body de un POST*/
        else if (strncasecmp(linea, "Content-Length:", 15) == 0){
            const char *valor = linea + 15;
            while (*valor == ' ' || *valor == '\t'){
                valor++;
            }
            req->content_length = atoi(valor);
        }

        /*Avanzar a la siguiente linea*/
        const char *siguiente = strstr(linea, "\r\n");
        if (siguiente == NULL){
            break;
        }
        linea = siguiente + 2;  
    }

    /*
     * Paso 6: Validacion HTTP/1.1: el header Host es obligatorio.
     * RFC 2616 14.23: "A client MUST include a Host header field in all
     * HTTP/1.1 request messages."
    */
    if (req->host[0] == '\0'){
        return -1;
    }

    /*
     * Paso 7: Apuntar body al inicio del cuerpo del mensaje.
     * El body empieza inmediatamente despues del separador \r\n\r\n.
    */
    req->body = (char *)(fin_headers + 4);

    return 0;
}
