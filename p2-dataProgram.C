#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h> 
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/shm.h>
#include <signal.h>

// Definiciones de tamaño para la tabla hash y rutas de archivos
#define TAMANO_TABLA 1200003 
#define ARCHIVO_DATASET "/media/farid/OS1/Users/farid/Downloads/archive/full_dataset_clean.csv"
//#define ARCHIVO_DATASET "full_dataset_clean.csv" 
#define ARCHIVO_INDICES "/media/farid/OS1/Users/farid/Downloads/archive/index.bin"
//#define ARCHIVO_INDICES "index.bin"

// Claves para la memoria compartida
#define LLAVE_SHM_FECHA 123
#define LLAVE_SHM_TIEMPO_INICIAL 456
#define LLAVE_SHM_TIEMPO_FINAL 986
#define LLAVE_SHM_IDIOMA 789
#define LLAVE_SHM_RESULTADOS 147

//
#define LIMITE_BUFFER_RESULTADOS 100000

// Rutas para las tuberías
#define RUTA_FIFO_INDICE_TERMINADO "fifo_indice_terminado"
#define RUTA_FIFO_RESULTADOS "fifo_resultados"
#define RUTA_FIFO_CONTINUIDAD "fifo_continuidad"

// posibles estados de continuidad para el proceso de consulta y apertura
#define ESTADO_CONTINUAR "c"
#define ESTADO_SALIR "s"

// VARIABLES GLOBALES

//descriptores de archivo y punteros a las memorias compartidas
int fd_indice_creado, fd_resultados, fd_continuidad, shm_id_fecha, shm_id_idioma, shm_id_hora_inicial, shm_id_hora_final;
char *ap_fecha_shm, *ap_idioma_shm, *ap_tiempo_inicial_shm, *ap_tiempo_final_shm;

// Buffers para almacenar los parámetros de búsqueda
char fecha[11], hora_inicial[9], hora_final[9], idioma[4];

// Estructura para representar un Tweet
struct Tweet {
    long id;
    char fecha[11];   // YYYY-MM-DD
    char tiempo[9];   // HH:MM:SS
    char idioma[4];   // ej: "es", "en"
    char pais[4];     // ej: "DE", "PA"
};

// Contiene un puntero apuntando a un registro en el CSV y un puntero al siguiente nodo de colisión.
struct NodoIndice { 
    int64_t offset_csv;       
    int64_t offset_siguiente_nodo; 
};

void leer_en_tuberia(int fd, void *receptor, int tamaño){

    int r;
    
    r = read(fd, receptor, tamaño);

    if (r != tamaño){
        if(r==-1){
            perror("Error al momento de leer");
        }else{
            printf("Error al momento de leer: no se ha podido leer todos los bytes.");
        }
        exit(-1);
    
    }

}

void escribir_en_tuberia(int fd, void *contenido, int tamaño){

    int r;
    
    r = write(fd, contenido, tamaño);

    if (r != tamaño){
        if(r==-1){
            perror("Error al momento de escribir:");
        }
        else{
            perror("Error al momento de escribir, no se escribieron todos los bytes:");
        }
        exit(-1);
    }

}

// Función Hash (utilizando el algoritmo djb2)
unsigned long hash_djb2(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

// Función para crear la tabla hash en disco (indexando el apuntador al registro en el CSV)
void crear_tabla_hash() {
    FILE *dataset_fd, *indice_fd;
    int64_t *tabla_indices; // Esta es la tabla de "cabeceras" de las listas enlazadas.

    printf("Asignando memoria para la tabla de cabeceras del índice (%.2f MB)...\n", (double)TAMANO_TABLA * sizeof(int64_t) / (1024*1024));
    tabla_indices = (int64_t *)malloc(TAMANO_TABLA * sizeof(int64_t));
    if (!tabla_indices) {
        perror("Error al asignar memoria para la tabla de índice");
        exit(EXIT_FAILURE);
    }
    
    /// Inicializar todas las cabeceras a -1, indicando listas vacías.
    for (size_t i = 0; i < TAMANO_TABLA; i++) {
        tabla_indices[i] = -1;
    }

    dataset_fd = fopen(ARCHIVO_DATASET, "r");
    if (!dataset_fd) {
        perror("Error al abrir el dataset de entrada");
        free(tabla_indices);
        exit(EXIT_FAILURE);
    }
    
    // Abrimos el archivo de índice en modo escritura binaria.
    indice_fd = fopen(ARCHIVO_INDICES, "wb");
    if (!indice_fd) {
        perror("Error al crear el archivo de índice binario");
        fclose(dataset_fd);
        free(tabla_indices);
        exit(EXIT_FAILURE);
    }
    
    // --- Creación del Índice ---

    // Escritura del espacio reservado para la tabla de cabeceras, que llenará al final.
    printf("Escribiendo cabecera de índice en %s...\n", ARCHIVO_INDICES);
    fwrite(tabla_indices, sizeof(int64_t), TAMANO_TABLA, indice_fd);

    printf("Procesando el dataset y construyendo el índice...\n");
    char buffer_linea[256];
    long long contador_tweets = 0;
    
    // Guardado del offset después de la cabecera del CSV para empezar a leer datos reales.
    int64_t offset_csv_actual = ftell(dataset_fd);

    while (fgets(buffer_linea, sizeof(buffer_linea), dataset_fd)) {
        char cadena_fecha[11] = {0};

        // Parseado de la fecha, necesaria para el hash
        if (sscanf(buffer_linea, "%*[^,],%10[^,]", cadena_fecha) != 1) {
            offset_csv_actual = ftell(dataset_fd); // Actualizar offset para la siguiente línea
            continue; // Si la línea no tiene el formato esperado, se salta.
        }

        //Calculo del hash y el índice para la fecha.
        unsigned long valor_hash = hash_djb2(cadena_fecha);
        unsigned int indice = valor_hash % TAMANO_TABLA;

        // Creación del nuevo nodo del índice.
        struct NodoIndice nuevo_nodo;
        nuevo_nodo.offset_csv = offset_csv_actual;      
        nuevo_nodo.offset_siguiente_nodo = tabla_indices[indice]; // El nuevo nodo apunta a la antigua cabecera de la lista.

        //Escribir el nuevo nodo al final del archivo de índice.
        fseek(indice_fd, 0, SEEK_END);
        int64_t new_node_offset = ftell(indice_fd); // Posición donde se escribe el nodo.
        fwrite(&nuevo_nodo, sizeof(struct NodoIndice), 1, indice_fd);

        // Actualizar la tabla de cabeceras en memoria para que apunte a este nuevo nodo.
        tabla_indices[indice] = new_node_offset;

        contador_tweets++;
        if (contador_tweets % 1000000 == 0) {
            printf("Procesados %lld millones de registros...\n", contador_tweets / 1000000);
        }
        
        // Preparar el offset para la siguiente iteración.
        offset_csv_actual = ftell(dataset_fd);
    }
    printf("Procesamiento completo. Total de registros indexados: %lld\n", contador_tweets);

    //Volver al inicio del archivo de índice y escribir la tabla de cabeceras final.
    printf("Escribiendo la tabla de cabeceras final en %s...\n", ARCHIVO_INDICES);
    fseek(indice_fd, 0, SEEK_SET);
    fwrite(tabla_indices, sizeof(int64_t), TAMANO_TABLA, indice_fd);
    
    printf("¡Creación del índice finalizada!\n");

    fclose(dataset_fd);
    fclose(indice_fd);
    free(tabla_indices);
}

void *inicializar_memoria_compartida(long tamano, key_t llave, int *shmId){

    void *ap;

    //crear segmento de memoria compartida
	*shmId = shmget(llave, tamano, 0666 | IPC_CREAT);
    
	//verificacion de error
	if (*shmId < 0){
    	perror("Error error al crear el segmento de memoria compartida:");
        exit(-1);
	}

    ap = shmat(*shmId, 0, 0);

    if (ap == NULL){
    	perror("Error al crear el apuntador:");
        exit(-1);
	}

    return ap;

}

void cerrar_memoria_compartida(void* ap, int shmId){

    int r;

    //se desprende el segmento de memoria compartida (la interfaz de cliente eliminará el segmento de memoria compartida)
    r = shmdt(ap);

    //verificacion del error
    if(r == -1){ 
        perror("Error al desprender la memoria compartida:");
        exit(-1);
    }
}

// Función para buscar en el CSV usando la tabla hash 
void busqueda(const char *fecha_busqueda, const char *tiempo_inicial_busqueda, const char *tiempo_final_busqueda, const char *idioma_busqueda) {
    FILE *indice_fd, *dataset_fd;
    int64_t *index_table;
    
    index_table = (int64_t *)malloc(TAMANO_TABLA * sizeof(int64_t));
    if (!index_table) {
        perror("Error al asignar memoria para la tabla de índice");
        return;
    }

    indice_fd = fopen(ARCHIVO_INDICES, "rb");
    if (!indice_fd) {
        perror("No se pudo abrir el archivo de índice. ¿Ejecutaste la creación primero?");
        free(index_table);
        return;
    }
    
    // Leer solo la tabla de cabeceras del archivo de índice.
    fread(index_table, sizeof(int64_t), TAMANO_TABLA, indice_fd);

    dataset_fd = fopen(ARCHIVO_DATASET, "r");
    if (!dataset_fd) {
        perror("No se pudo abrir el archivo de datos CSV");
        fclose(indice_fd);
        free(index_table);
        return;
    }

    // Calcular el índice hash para la fecha de búsqueda.
    unsigned long valor_hash = hash_djb2(fecha_busqueda);
    unsigned int indice = valor_hash % TAMANO_TABLA;
    int64_t offset_nodo_actual = index_table[indice];

    // Si la cabecera es -1, no hay entradas para este hash.
    if (offset_nodo_actual == -1) {
        int concidencias = 0;

        //r = write(fd_resultados, &concidencias, sizeof(int));
        escribir_en_tuberia(fd_resultados, &concidencias, sizeof(concidencias));

    } else {
        int cantidad_coincidencias = 0, shm_id_resultados;
        struct NodoIndice nodo_actual;
        char buffer_linea[256];

        struct Tweet buffer[LIMITE_BUFFER_RESULTADOS];

        struct Tweet *ap_tweets;
        
        // Recorrido de la lista enlazada de nodos dentro del archivo de índice para manejar colisiones.
        while (offset_nodo_actual != -1 && cantidad_coincidencias < LIMITE_BUFFER_RESULTADOS) {
            // Se salta a la posición del nodo actual en el archivo de índice.
            fseek(indice_fd, offset_nodo_actual, SEEK_SET);
            fread(&nodo_actual, sizeof(struct NodoIndice), 1, indice_fd);

            // Se salta a la posición del registro en el archivo CSV.
            fseek(dataset_fd, nodo_actual.offset_csv, SEEK_SET);
            fgets(buffer_linea, sizeof(buffer_linea), dataset_fd);
            
            // Se parsea la línea del CSV para verificar la fecha (evitar colisiones).
            struct Tweet tweet = {0};
            char *token_dataset;
            char *puntero_guardado;
            buffer_linea[strcspn(buffer_linea, "\n")] = 0; // eliminar salto de línea

            token_dataset = strtok_r(buffer_linea, ",", &puntero_guardado);
            if (token_dataset) tweet.id = atoll(token_dataset);
            token_dataset = strtok_r(NULL, ",", &puntero_guardado);
            if (token_dataset) strncpy(tweet.fecha, token_dataset, sizeof(tweet.fecha) - 1);
            token_dataset = strtok_r(NULL, ",", &puntero_guardado);
            if (token_dataset) strncpy(tweet.tiempo, token_dataset, sizeof(tweet.tiempo) - 1);
            token_dataset = strtok_r(NULL, ",", &puntero_guardado);
            if (token_dataset) strncpy(tweet.idioma, token_dataset, sizeof(tweet.idioma) - 1);
            token_dataset = strtok_r(NULL, "\n\r", &puntero_guardado);
            if (token_dataset) strncpy(tweet.pais, token_dataset, sizeof(tweet.pais) - 1);

             // Verificar si el registro cumple con todos los criterios de búsqueda.
            if (strcmp(tweet.fecha, fecha_busqueda) == 0 &&
                strcmp(tweet.idioma, idioma_busqueda) == 0 &&
                strcmp(tweet.tiempo, tiempo_inicial_busqueda) >= 0 &&  // Debe ser mayor o igual a la hora inicial
                strcmp(tweet.tiempo, tiempo_final_busqueda) <= 0)      // Debe ser menor o igual a la hora final
            {    
                buffer[cantidad_coincidencias] = tweet;
                cantidad_coincidencias++;
            }
            
            // Se mueve al siguiente nodo en la lista enlazada (cadena de colisión).
            offset_nodo_actual = nodo_actual.offset_siguiente_nodo;
        }

        if(cantidad_coincidencias == 0){

            escribir_en_tuberia(fd_resultados, &cantidad_coincidencias, sizeof(cantidad_coincidencias));

        } else {

            //Inicialización de memoria compartida
            ap_tweets = (struct Tweet*) inicializar_memoria_compartida(sizeof(struct Tweet)*cantidad_coincidencias, LLAVE_SHM_RESULTADOS, &shm_id_resultados);

            memcpy(ap_tweets, buffer, sizeof(struct Tweet)*cantidad_coincidencias);

            cerrar_memoria_compartida(ap_tweets, shm_id_resultados);

            // Enviar la cantidad de resultados encontrados a través de la tubería.
            //write(fd_resultados, &cantidad_coincidencias, sizeof(int));

            escribir_en_tuberia(fd_resultados, &cantidad_coincidencias, sizeof(cantidad_coincidencias));

        }

    }

    fclose(dataset_fd);
    fclose(indice_fd);
    free(index_table);
}



int existe_archivo(const char *path) {
    return access(path, F_OK) == 0;
}

int abrir_tuberia_escritura(const char *fifo_path){

    int fd = open(fifo_path, O_WRONLY);

    if(fd == -1){
        perror("Error al abrir la tuberia");
        exit(-1);
    }

    return fd;

}

int abrir_tuberia_lectura(const char *fifo_path){

    int fd = open(fifo_path, O_RDONLY);

    if(fd == -1){
        perror("Error al abrir la tuberia");
        exit(-1);
    }

    return fd;

}

void cerrar_proceso(){

    cerrar_memoria_compartida(ap_fecha_shm, shm_id_fecha);
    cerrar_memoria_compartida(ap_tiempo_inicial_shm, shm_id_hora_inicial);
    cerrar_memoria_compartida(ap_tiempo_final_shm, shm_id_hora_final);
    cerrar_memoria_compartida(ap_idioma_shm, shm_id_idioma);
    close(fd_indice_creado);
    close(fd_resultados);
    close(fd_continuidad);

    kill(getpid(),SIGTERM);

}


int main() {

    signal(SIGINT, NULL);

    sleep(1);

    char testigo[2] = "x";

    fd_continuidad = abrir_tuberia_lectura(RUTA_FIFO_CONTINUIDAD);

    leer_en_tuberia(fd_continuidad, testigo, sizeof(testigo));

    fd_indice_creado = abrir_tuberia_escritura(RUTA_FIFO_INDICE_TERMINADO);
    fd_resultados = abrir_tuberia_escritura(RUTA_FIFO_RESULTADOS);

    ap_fecha_shm = (char*) inicializar_memoria_compartida(sizeof(char[11]), LLAVE_SHM_FECHA, &shm_id_fecha);
    ap_tiempo_inicial_shm = (char*) inicializar_memoria_compartida(sizeof(char[9]), LLAVE_SHM_TIEMPO_INICIAL, &shm_id_hora_inicial);
    ap_tiempo_final_shm = (char*) inicializar_memoria_compartida(sizeof(char[9]), LLAVE_SHM_TIEMPO_FINAL, &shm_id_hora_final);
    ap_idioma_shm = (char*) inicializar_memoria_compartida(sizeof(char[4]), LLAVE_SHM_IDIOMA, &shm_id_idioma);
    
    if(!existe_archivo(ARCHIVO_INDICES)){
        crear_tabla_hash();
    }
    
    //write(fd_inicio, testigo, sizeof(testigo)); 
    escribir_en_tuberia(fd_indice_creado, testigo, sizeof(testigo));

    while(1) {
    
        //r = read(fd_buscar, testigo, sizeof(testigo)); 
        leer_en_tuberia(fd_continuidad, testigo, sizeof(testigo));

        if(strcmp(testigo, ESTADO_SALIR) == 0){

            cerrar_proceso();

        }
        
        strcpy(fecha, ap_fecha_shm);
        strcpy(hora_inicial, ap_tiempo_inicial_shm);
        strcpy(hora_final, ap_tiempo_final_shm);
        strcpy(idioma, ap_idioma_shm);

        //printf("fecha: %s\n", fecha);
        //printf("hora inicio: %s\n", hora_inicial);
        //printf("hora final: %s\n", hora_final);
        //printf("idioma: %s\n", idioma);

        busqueda(fecha, hora_inicial, hora_final, idioma);

        sleep(1);
    
    }

    return 0;
}