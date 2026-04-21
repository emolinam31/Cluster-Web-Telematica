/*
 * =============================================================================
 * PIBL - http_parser.c - Parser HTTP/1.1 para el proxy (RFC 2616)
 * =============================================================================
 *
 * REQUISITOS QUE DEBE CUMPLIR ESTE ARCHIVO:
 *   1. http_find_header(): Buscar un header especifico en un buffer HTTP.
 *      - Case-insensitive segun RFC 2616 seccion 4.2
 *      - Retorna puntero al inicio del valor o NULL si no existe
 *   2. http_get_content_length(): Wrapper sobre http_find_header que retorna
 *      el valor de Content-Length como entero. Sirve para que el proxy sepa
 *      cuantos bytes leer del backend antes de cerrar la conexion.
 *   3. http_parse_request(): Parsear la peticion del cliente antes de
 *      reenviarla al backend. Misma logica que el WS.
 *
 * CLAVES PARA EL EXITO:
 *   - Los headers son case-insensitive: "Content-Length:" == "content-length:"
 *   - El PIBL usa http_find_header sobre RESPUESTAS del backend,
 *     no solo sobre peticiones del cliente.
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

/*
 * Busca un header especifico dentro del buffer HTTP.
 * Recorre linea por linea hasta el separador \r\n\r\n comparando el inicio
 * de cada linea con "header_name:" de forma case-insensitive.
 *
 * Retorna: puntero al primer caracter del valor, o NULL si no se encuentra.
 */
const char *http_find_header(const char *buffer, const char *header_name){
    if (buffer == NULL || header_name == NULL){
        return NULL;
    }

    size_t nombre_len = strlen(header_name);

    /* Empezar despues del primer CRLF, que cierra la Request-Line o Status-Line */
    const char *linea = strstr(buffer, "\r\n");
    if (linea == NULL){
        return NULL;
    }
    linea += 2;

    const char *fin_headers = strstr(buffer, "\r\n\r\n");
    if (fin_headers == NULL){
        return NULL;
    }

    while (linea < fin_headers){
        /* La linea debe empezar con "header_name" seguido de ':' */
        if (strncasecmp(linea, header_name, nombre_len) == 0 && linea[nombre_len] == ':'){
            const char *valor = linea + nombre_len + 1;
            while (*valor == ' ' || *valor == '\t'){
                valor++;
            }
            return valor;
        }

        /*Avanzar a la siguiente linea*/
        const char *siguiente = strstr(linea, "\r\n");
        if (siguiente == NULL){
            break;
        }
        linea = siguiente + 2;
    }

    return NULL;
}

/*
 * Wrapper sobre http_find_header para extraer Content-Length como entero.
 * Retorna: valor del header o -1 si no existe.
 */
int http_get_content_length(const char *buffer){
    const char *valor = http_find_header(buffer, "Content-Length");
    if (valor == NULL){
        return -1;
    }
    return atoi(valor);
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
    if (sscanf(buffer,"%7s %2047s %15s", req->method, req->uri, req->version) != 3){
        return -1;
    }

    /*
    Paso 2: el metodo deber ser GET, HEAD o POST
    */
    if (!metodo_soportado(req->method)){
        return -1;
    }

    /*
    Paso 3: La version debe ser HTTP/1.1
    */
    if (!version_soportada(req->version)){
        return -1;
    }

    /*
    Paso 4: Localizar el separador headers/body
    * RFC 2616 seccion 4.1: una linea vacia (CRLF CRLF) marca el fin de los
    * headers y el inicio del body.
    */
    const char *fin_headers = strstr(buffer, "\r\n\r\n");
    if (fin_headers == NULL){
        return -1;
    }

    /*
     * Paso 5: Extraer Host y Content-Length usando http_find_header.
     * Reutilizamos el helper en lugar de duplicar el loop de headers.
    */
    const char *valor_host = http_find_header(buffer, "Host");
    if (valor_host != NULL){
        sscanf(valor_host, "%255[^\r\n]", req->host);
    }

    int cl = http_get_content_length(buffer);
    if (cl >= 0){
        req->content_length = cl;
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
