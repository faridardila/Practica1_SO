#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <signal.h>

// opciones del menu
#define INGRESAR_PRIMER_CRITERIO 1
#define INGRESAR_SEGUNDO_CRITERIO 2
#define INGRESAR_TERCER_CRITERIO 3
#define BUSCAR 4
#define VER_CRITERIOS 5
#define SALIR 6

// llaves para memoira compartida
#define LLAVE_SHM_FECHA 123
#define LLAVE_SHM_TIEMPO_INICIAL 456
#define LLAVE_SHM_TIEMPO_FINAL 986
#define LLAVE_SHM_IDIOMA 789
#define LLAVE_SHM_RESULTADOS 147

// medidas de ancho para la impresion de resultados
#define ANCHO_ID 20
#define ANCHO_FECHA 12
#define ANCHO_TIEMPO 10
#define ANCHO_IDIOMA 8
#define ANCHO_PAIS 8

// rutas para la creación de tuberias
#define RUTA_FIFO_INDICE_TERMINADO "fifo_indice_terminado"
#define RUTA_FIFO_RESULTADOS "fifo_resultados"
#define RUTA_FIFO_CONTINUIDAD "fifo_continuidad"

// posibles estados de continuidad para el proceso de consulta y apertura
#define ESTADO_CONTINUAR "c"
#define ESTADO_SALIR "s"

// cirterios ingresados por el usuario para la busqueda de tweets
char criterio_fecha[11], criterio_idioma[4], criterio_tiempo_inicio[9], criterio_tiempo_final[9];

// descriptores de tuberias
int fd_indice_creado, fd_resultados, fd_continuidad, shm_id_resultados;

// id para la creación de memoria compartida
int id_shm_fecha, id_shm_idioma, id_shm_tiempo_inicial, id_shm_tiempo_final;

// apuntadores para el manejo de memoria compartida
char *ap_fecha, *ap_idioma, *ap_tiempo_inicial, *ap_tiempo_final;

struct Tweet{
    long long id;
    char fecha[11];  // YYYY-MM-DD
    char tiempo[9];   // HH:MM:SS
    char idioma[4];   // ej: "es", "en", "und"
    char pais[4];   // ej: "DE", "RU"
};

void mostrar_menu(){

    printf("\n1. Ingresar primer criterio de busqueda (fecha).\n");
    printf("2. Ingresar segundo criterio de busqueda (rango de tiempo).\n");
    printf("3. Ingresar tercer criterio de busqueda (idioma).\n");
    printf("4. Realizar busqueda.\n");
    printf("5. Ver criterios.\n");
    printf("6. Salir.\n");

}

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

void *inicializar_memoria_compartida(long tamaño, key_t llave, int *id_shm){

    void *ap;

    //crear segmento de memoria compartida
	*id_shm = shmget(llave, tamaño, 0666 | IPC_CREAT);
    
	//verificacion de error
	if (*id_shm < 0){
    	perror("Error error al crear el segmento de memoria compartida:");
        exit(-1);
	}

    ap = shmat(*id_shm, 0, 0);

    if (ap == NULL){
    	perror("Error al crear el apuntador:");
        exit(-1);
	}

    return ap;

}

void cerrar_memoria_compartida(void* ap, int id_shm){

    int r;

    //se desprende el segmento de memoria compartida
    r = shmdt(ap);

    //verificacion del error
    if(r == -1){ //r < 0
        perror("Error al desprender la memoria compartida:");
        exit(-1);
    }

    //eliminación del segmento de memoria compartida
    r = shmctl(id_shm, IPC_RMID, NULL);

    //verificacion del error
    if(r == -1){
        perror("Error al eliminar el segmento de memoria compartida:");
        exit(-1);
    }

}

// Funciones auxiliares (mantenerlas como están, solo para contexto)
bool es_bisiesto(int año) {
    return (año % 4 == 0 && año % 100 != 0) || (año % 400 == 0);
}

int dias_del_mes(int mes, int año) {
    if (mes == 2) {
        return es_bisiesto(año) ? 29 : 28;
    }
    if (mes == 4 || mes == 6 || mes == 9 || mes == 11) {
        return 30;
    }
    return 31;
}

// limpiar_buffer is not strictly needed with fgets/sscanf, but kept for completeness.
void limpiar_buffer() {
    int caracter;
    while ((caracter = getchar()) != '\n' && caracter != EOF);
}

void ingresar_primer_criterio() {

    limpiar_buffer();

    int dia, mes, año;
    char linea[100]; // Buffer para leer la línea completa
    char caracter_extra; // Para verificar si hay caracteres extra después del número
    int resultado_del_escaneo; // To store the result of sscanf

    time_t ahora = time(NULL);
    struct tm *info_tiempo = localtime(&ahora);
    int año_actual = info_tiempo->tm_year + 1900;

    // Ingresar año
    do {
        printf("Por favor, ingrese el año (entre 2019 y %d):\n", año_actual);
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            printf("Error al leer la entrada.\n");
            exit(1);
        }

        // Try to read an integer and then check for an extra character
        resultado_del_escaneo = sscanf(linea, "%d %c", &año, &caracter_extra); // Note the space after %d

        if (resultado_del_escaneo == 2) { // Read a number AND another character
            if (caracter_extra != '\n') { // If the extra character is NOT a newline, it's invalid
                printf("Entrada no válida. Debe ingresar solo un número entero. (Ej: 2019, no 2019k)\n");
                año = -1; // Set to invalid value to force re-loop
            }
            // If extra_char IS '\n', it means something like "2006 \n" which is okay.
            // The value of ano is valid, so proceed to range check.
        } else if (resultado_del_escaneo == 1) { // Read only a number (e.g., "2006\n")
            // This is a valid numeric input. Proceed to range check.
        } else { // Couldn't read a number at all (e.g., "abc")
            printf("Entrada no válida. Debe ingresar un número entero.\n");
            año = -1; // Set to invalid value to force re-loop
        }

        // Now, check the range if a valid number was parsed
        if (año != -1 && (año < 2019 || año > año_actual)) {
            printf("El año %d no es válido. Debe estar entre 2019 y %d.\n", año, año_actual);
            año = -1; // Set to invalid value to force re-loop
        }
    } while (año == -1); // Loop until 'ano' is a valid number within range

    // Ingresar mes
    do {
        printf("Por favor, ingrese el mes (entre 1 y 12):\n");
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            printf("Error al leer la entrada.\n");
            exit(1);
        }

        resultado_del_escaneo = sscanf(linea, "%d %c", &mes, &caracter_extra);

        if (resultado_del_escaneo == 2) {
            if (caracter_extra != '\n') {
                printf("Entrada no válida. Debe ingresar solo un número entero.\n");
                mes = -1;
            }
        } else if (resultado_del_escaneo == 1) {
            // Valido
        } else {
            printf("Entrada no válida. Debe ingresar un número entero.\n");
            mes = -1;
        }

        if (mes != -1 && (mes < 1 || mes > 12)) {
            printf("El mes %d no es válido. Debe estar entre 1 y 12.\n", mes);
            mes = -1;
        }
    } while (mes == -1);

    // Ingresar día
    int dias_mes = dias_del_mes(mes, año);
    do {
        printf("Por favor, ingrese el día (entre 1 y %d):\n", dias_mes);
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            printf("Error al leer la entrada.\n");
            exit(1);
        }

        resultado_del_escaneo = sscanf(linea, "%d %c", &dia, &caracter_extra);

        if (resultado_del_escaneo == 2) {
            if (caracter_extra != '\n') {
                printf("Entrada no válida. Debe ingresar solo un número entero.\n");
                dia = -1;
            }
        } else if (resultado_del_escaneo == 1) {
            // Valid
        } else {
            printf("Entrada no válida. Debe ingresar un número entero.\n");
            dia = -1;
        }

        if (dia != -1 && (dia < 1 || dia > dias_mes)) {
            printf("El día %d no es válido. Debe estar entre 1 y %d.\n", dia, dias_mes);
            dia = -1;
        }
    } while (dia == -1);

    sprintf(criterio_fecha, "%04d-%02d-%02d", año, mes, dia);
}

void ingresar_segundo_criterio(){

    limpiar_buffer();

    int hora_inicial, minuto_inicial, segundo_inicial, hora_final, minuto_final, segundo_final;
    char linea[100]; // Buffer para leer la línea completa
    char caracter_extra; // Para verificar si hay caracteres extra después del número
    int resultado_del_escaneo; // To store the result of sscanf
    bool fecha_final_menor = false;

    // Ingresar hora_inicial
    do {
        printf("Por favor, ingrese la hora inicial (entre 0 y 23)\n");
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            printf("Error al leer la entrada.\n");
            exit(1);
        }

        // Try to read an integer and then check for an extra character
        resultado_del_escaneo = sscanf(linea, "%d %c", &hora_inicial, &caracter_extra); // Note the space after %d

        if (resultado_del_escaneo == 2) { // Read a number AND another character
            if (caracter_extra != '\n') { // If the extra character is NOT a newline, it's invalid
                printf("Entrada no válida. Debe ingresar solo un número entero. (Ej: 12, no 12k)\n");
                hora_inicial = -1; // Set to invalid value to force re-loop
            }
            // If extra_char IS '\n', it means something like "2006 \n" which is okay.
            // The value of ano is valid, so proceed to range check.
        } else if (resultado_del_escaneo == 1) { // Read only a number (e.g., "2006\n")
            // This is a valid numeric input. Proceed to range check.
        } else { // Couldn't read a number at all (e.g., "abc")
            printf("Entrada no válida. Debe ingresar un número entero.\n");
            hora_inicial = -1; // Set to invalid value to force re-loop
        }

        // Now, check the range if a valid number was parsed
        if (hora_inicial != -1 && (hora_inicial < 0 || hora_inicial > 23)) {
            printf("La hora inicial %d no es valida. Debe estar entre 0 y 23.\n", hora_inicial);
            hora_inicial = -1; // Set to invalid value to force re-loop
        }
    } while (hora_inicial == -1); // Loop until 'ano' is a valid number within range

    // Ingresar mes
    do {
        printf("Por favor, ingrese el minuto inicial (entre 0 y 59):\n");
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            printf("Error al leer la entrada.\n");
            exit(1);
        }

        resultado_del_escaneo = sscanf(linea, "%d %c", &minuto_inicial, &caracter_extra);

        if (resultado_del_escaneo == 2) {
            if (caracter_extra != '\n') {
                printf("Entrada no válida. Debe ingresar solo un número entero.\n");
                minuto_inicial = -1;
            }
        } else if (resultado_del_escaneo == 1) {
            // Valido
        } else {
            printf("Entrada no válida. Debe ingresar un número entero.\n");
            minuto_inicial = -1;
        }

        if (minuto_inicial != -1 && (minuto_inicial < 0 || minuto_inicial > 59)) {
            printf("El minuto inicial %d no es válido. Debe estar entre 0 y 59.\n", minuto_inicial);
            minuto_inicial = -1;
        }
    } while (minuto_inicial == -1);

    // Ingresar día
    do {
        printf("Por favor, ingrese el segundo inicial (entre 0 y 59):\n");
        if (fgets(linea, sizeof(linea), stdin) == NULL) {
            printf("Error al leer la entrada.\n");
            exit(1);
        }

        resultado_del_escaneo = sscanf(linea, "%d %c", &segundo_inicial, &caracter_extra);

        if (resultado_del_escaneo == 2) {
            if (caracter_extra != '\n') {
                printf("Entrada no válida. Debe ingresar solo un número entero.\n");
                segundo_inicial = -1;
            }
        } else if (resultado_del_escaneo == 1) {
            // Valid
        } else {
            printf("Entrada no válida. Debe ingresar un número entero.\n");
            segundo_inicial = -1;
        }

        if (segundo_inicial != -1 && (segundo_inicial < 0 || segundo_inicial > 59)) {
            printf("El segundo inicial %d no es válido. Debe estar entre 0 y 59.\n", segundo_inicial);
            segundo_inicial = -1;
        }
    } while (segundo_inicial == -1);

    sprintf(criterio_tiempo_inicio, "%02d:%02d:%02d", hora_inicial, minuto_inicial, segundo_inicial);



    do{

        if(fecha_final_menor){
            printf("La fecha final debe ser posterior a la fecha inicial\n");
        }

        // Ingresar hora_inicial
        do {
            printf("Por favor, ingrese la hora final (entre 0 y 23)\n");
            if (fgets(linea, sizeof(linea), stdin) == NULL) {
                printf("Error al leer la entrada.\n");
                exit(1);
            }

            // Try to read an integer and then check for an extra character
            resultado_del_escaneo = sscanf(linea, "%d %c", &hora_final, &caracter_extra); // Note the space after %d

            if (resultado_del_escaneo == 2) { // Read a number AND another character
                if (caracter_extra != '\n') { // If the extra character is NOT a newline, it's invalid
                    printf("Entrada no válida. Debe ingresar solo un número entero. (Ej: 12, no 12k)\n");
                    hora_final = -1; // Set to invalid value to force re-loop
                }
                // If extra_char IS '\n', it means something like "2006 \n" which is okay.
                // The value of ano is valid, so proceed to range check.
            } else if (resultado_del_escaneo == 1) { // Read only a number (e.g., "2006\n")
                // This is a valid numeric input. Proceed to range check.
            } else { // Couldn't read a number at all (e.g., "abc")
                printf("Entrada no válida. Debe ingresar un número entero.\n");
                hora_final = -1; // Set to invalid value to force re-loop
            }

            // Now, check the range if a valid number was parsed
            if (hora_final != -1 && (hora_final < 0 || hora_final > 23)) {
                printf("La hora final %d no es valida. Debe estar entre 0 y 23.\n", hora_final);
                hora_final = -1; // Set to invalid value to force re-loop
            }
        } while (hora_final == -1); // Loop until 'ano' is a valid number within range

        // Ingresar mes
        do {
            printf("Por favor, ingrese el minuto final (entre 0 y 59):\n");
            if (fgets(linea, sizeof(linea), stdin) == NULL) {
                printf("Error al leer la entrada.\n");
                exit(1);
            }

            resultado_del_escaneo = sscanf(linea, "%d %c", &minuto_final, &caracter_extra);

            if (resultado_del_escaneo == 2) {
                if (caracter_extra != '\n') {
                    printf("Entrada no válida. Debe ingresar solo un número entero.\n");
                    minuto_final = -1;
                }
            } else if (resultado_del_escaneo == 1) {
                // Valido
            } else {
                printf("Entrada no válida. Debe ingresar un número entero.\n");
                minuto_final = -1;
            }

            if (minuto_final != -1 && (minuto_final < 0 || minuto_final > 59)) {
                printf("El minuto final %d no es válido. Debe estar entre 0 y 59.\n", minuto_final);
                minuto_final = -1;
            }
        } while (minuto_final == -1);

        // Ingresar día
        do {
            printf("Por favor, ingrese el segundo final (entre 0 y 59):\n");
            if (fgets(linea, sizeof(linea), stdin) == NULL) {
                printf("Error al leer la entrada.\n");
                exit(1);
            }

            resultado_del_escaneo = sscanf(linea, "%d %c", &segundo_final, &caracter_extra);

            if (resultado_del_escaneo == 2) {
                if (caracter_extra != '\n') {
                    printf("Entrada no válida. Debe ingresar solo un número entero.\n");
                    segundo_final = -1;
                }
            } else if (resultado_del_escaneo == 1) {
                // Valid
            } else {
                printf("Entrada no válida. Debe ingresar un número entero.\n");
                segundo_final = -1;
            }

            if (segundo_final != -1 && (segundo_final < 0 || segundo_final > 59)) {
                printf("El segundo final %d no es válido. Debe estar entre 0 y 59.\n", segundo_final);
                segundo_final = -1;
            }
        } while (segundo_final == -1);

        sprintf(criterio_tiempo_final, "%02d:%02d:%02d", hora_final, minuto_final, segundo_final);

        fecha_final_menor = true;

    } while (strcmp(criterio_tiempo_final, criterio_tiempo_inicio) < 0);

    
}

void ingresar_tercer_criterio() {

    char idioma_temporal[4] = {0};
    char caracter;
        
    limpiar_buffer();
    
    while (true) {
        printf("Por favor, ingrese el código de idioma (2-3 letras): ");
                
        // Leer exactamente 3 caracteres del búfer
        int cantidad_de_caracteres = 0;
        while (1) {
            caracter = getchar();
            if (caracter == '\n' || caracter == EOF) {
                if(cantidad_de_caracteres < 4)
                    idioma_temporal[cantidad_de_caracteres] = '\0';
                break;
            }
            
            if(cantidad_de_caracteres < 4)
                idioma_temporal[cantidad_de_caracteres] = caracter;

            cantidad_de_caracteres++;
        }
        
        // Validar longitud (2 o 3 caracteres)
        if (cantidad_de_caracteres < 2 || cantidad_de_caracteres > 3) {
            printf("Debe ingresar exactamente 2 o 3 letras.\n");
            continue;
        }
        
        // Validar que sean solo letras
        bool es_valido = true;
        for (int indice_caracter_actual = 0; indice_caracter_actual < cantidad_de_caracteres; indice_caracter_actual++) {
            if (!isalpha((unsigned char)idioma_temporal[indice_caracter_actual])) {
                es_valido = false;
                break;
            }
        }
        
        if (!es_valido) {
            printf("Solo se permiten letras (a-z, A-Z).\n");
            continue;
        }
        
        // Convertir a minúsculas
        for (int indice_caracter_actual = 0; indice_caracter_actual < cantidad_de_caracteres; indice_caracter_actual++) {
            idioma_temporal[indice_caracter_actual] = tolower((unsigned char)idioma_temporal[indice_caracter_actual]);
        }
        
        strcpy(criterio_idioma, idioma_temporal);
        break;
    }
}

void buscar(){

    char testigo[2] = ESTADO_CONTINUAR;
    struct Tweet *ap_tweet;
    time_t inicio, fin;
    int cantidad_resultados;

    strcpy(ap_fecha, criterio_fecha);
    strcpy(ap_tiempo_inicial, criterio_tiempo_inicio);
    strcpy(ap_tiempo_final, criterio_tiempo_final);
    strcpy(ap_idioma, criterio_idioma);

    inicio = clock();

    escribir_en_tuberia(fd_continuidad, testigo, sizeof(testigo));

    leer_en_tuberia(fd_resultados, &cantidad_resultados, sizeof(cantidad_resultados));

    if(cantidad_resultados > 0){

        ap_tweet = (struct Tweet *) inicializar_memoria_compartida(sizeof(struct Tweet)*cantidad_resultados, LLAVE_SHM_RESULTADOS, &shm_id_resultados);

// Define column widths for better alignment

        // Print top border
        printf("\n+");
        for (int i = 0; i < ANCHO_ID + 2; i++) printf("-"); // +2 for padding
        printf("+");
        for (int i = 0; i < ANCHO_FECHA + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_TIEMPO + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_IDIOMA + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_PAIS + 2; i++) printf("-");
        printf("+\n");

        // Print header row
        printf("| %-*s | %-*s | %-*s | %-*s | %-*s |\n",
               ANCHO_ID, "ID",
               ANCHO_FECHA, "Fecha",
               ANCHO_TIEMPO, "Tiempo",
               ANCHO_IDIOMA, "Idioma",
               ANCHO_PAIS, "Pais");

        // Print separator line after header
        printf("+");
        for (int i = 0; i < ANCHO_ID + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_FECHA + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_TIEMPO + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_IDIOMA + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_PAIS + 2; i++) printf("-");
        printf("+\n");

        // Print data rows
        for(int posicion_tweet_actual = 0; posicion_tweet_actual < cantidad_resultados; posicion_tweet_actual++){

            if(strlen(ap_tweet[posicion_tweet_actual].pais) == 0){
                strcpy(ap_tweet[posicion_tweet_actual].pais, "-");
            }

            printf("| %-*lld | %-*s | %-*s | %-*s | %-*s |\n", 
                   ANCHO_ID, ap_tweet[posicion_tweet_actual].id,
                   ANCHO_FECHA, ap_tweet[posicion_tweet_actual].fecha,
                   ANCHO_TIEMPO, ap_tweet[posicion_tweet_actual].tiempo,
                   ANCHO_IDIOMA, ap_tweet[posicion_tweet_actual].idioma,
                   ANCHO_PAIS, ap_tweet[posicion_tweet_actual].pais);
        }

        // Print bottom border
        printf("+");
        for (int i = 0; i < ANCHO_ID + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_FECHA + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_TIEMPO + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_IDIOMA + 2; i++) printf("-");
        printf("+");
        for (int i = 0; i < ANCHO_PAIS + 2; i++) printf("-");
        printf("+\n");

        fin = clock();

        cerrar_memoria_compartida(ap_tweet, shm_id_resultados);

        printf("Se han encontrado %d resultados en %.20lf segundos.\n", cantidad_resultados, (double)(fin - inicio)/CLOCKS_PER_SEC);

    } else {
        printf("\nNA\n");
    }

}

void cerrar_proceso(){

    char testigo[2] = ESTADO_SALIR;

    escribir_en_tuberia(fd_continuidad, testigo, sizeof(testigo));

    cerrar_memoria_compartida(ap_fecha, id_shm_fecha);
    cerrar_memoria_compartida(ap_tiempo_inicial, id_shm_tiempo_inicial);
    cerrar_memoria_compartida(ap_tiempo_final, id_shm_tiempo_final);
    cerrar_memoria_compartida(ap_idioma, id_shm_idioma);
    close(fd_indice_creado);
    close(fd_resultados);
    close(fd_continuidad);

}

void salir(){

    printf("Saliendo...\n");

    cerrar_proceso();
    exit(0);
}

int abrir_tuberia_para_escritura(const char *fifo_path){

    int fd = open(fifo_path, O_WRONLY);

    if(fd == -1){
        perror("Error al abrir la tuberia");
        exit(-1);
    }

    return fd;

}

int abrir_tuberia_para_lectura(const char *fifo_path){

    int fd = open(fifo_path, O_RDONLY);

    if(fd == -1){
        perror("Error al abrir la tuberia");
        exit(-1);
    }

    return fd;

}

void señal_de_cierre(int sig){

    printf("\nSaliendo...\n");

    cerrar_proceso();

    kill(getpid(), SIGTERM);

}

void inicializar_tuberias(){

    int r;

    // Eliminación de tuberías existentes para asegurar un inicio limpio.
    unlink(RUTA_FIFO_INDICE_TERMINADO);
    unlink(RUTA_FIFO_RESULTADOS);
    unlink(RUTA_FIFO_CONTINUIDAD);

    // Creación de las tuberías con permisos de lectura/escritura para todos.
    r = mkfifo(RUTA_FIFO_INDICE_TERMINADO, 0666);
    //verificación del error
    if(r == -1){
        perror("Error al crear la tuberia:");
        exit(-1);
    }

    r = mkfifo(RUTA_FIFO_RESULTADOS, 0666);
    //verificación del error
    if(r == -1){
        perror("Error al crear la tuberia:");
        exit(-1);
    }

    r = mkfifo(RUTA_FIFO_CONTINUIDAD, 0666);
    //verificación del error
    if(r == -1){
        perror("Error al crear la tuberia:");
        exit(-1);
    }

}

void ver_criterios(){

    if(criterio_fecha[0] == '\0'){
        printf("\nNo se ha establecido una fecha.\n");
    } else {
        printf("\nFecha: %s\n", criterio_fecha);
    }

    if(criterio_tiempo_inicio[0] == '\0'){
        printf("No se ha establecido un tiempo inicial.\n");
    } else {
        printf("Tiempo inicial: %s\n", criterio_tiempo_inicio);
    }

    if(criterio_tiempo_final[0] == '\0'){
        printf("No se ha establecido un tiempo final.\n");
    } else {
        printf("Tiempo final: %s\n", criterio_tiempo_final);
    }

    if(criterio_idioma[0] == '\0'){
        printf("No se ha establecido un idioma.\n");
    } else {
        printf("Idioma: %s\n", criterio_idioma);
    }

}

int main(){

    signal(SIGINT, señal_de_cierre);

    char testigo[2] = ESTADO_CONTINUAR;
    bool informacion_faltante;

    inicializar_tuberias();

    fd_continuidad = abrir_tuberia_para_escritura(RUTA_FIFO_CONTINUIDAD);

    escribir_en_tuberia(fd_continuidad, testigo, sizeof(testigo));

    fd_indice_creado = abrir_tuberia_para_lectura(RUTA_FIFO_INDICE_TERMINADO);
    fd_resultados = abrir_tuberia_para_lectura(RUTA_FIFO_RESULTADOS);

    leer_en_tuberia(fd_indice_creado, testigo, sizeof(testigo)); //no olvidar el error

    ap_fecha = (char*) inicializar_memoria_compartida(sizeof(criterio_fecha), LLAVE_SHM_FECHA, &id_shm_fecha);
    ap_tiempo_inicial = (char*) inicializar_memoria_compartida(sizeof(criterio_tiempo_inicio), LLAVE_SHM_TIEMPO_INICIAL, &id_shm_tiempo_inicial);
    ap_tiempo_final = (char*) inicializar_memoria_compartida(sizeof(criterio_tiempo_final), LLAVE_SHM_TIEMPO_FINAL, &id_shm_tiempo_final);
    ap_idioma = (char*) inicializar_memoria_compartida(sizeof(criterio_idioma), LLAVE_SHM_IDIOMA, &id_shm_idioma);

    printf("\nBienvenido\n");

    while(1){

        mostrar_menu();

        int opcion;
        int resultado_del_escaneo = scanf("%d", &opcion); //verificar el contenido?

        if(resultado_del_escaneo != 1){

            printf("Entrada incorrecta, por favor, ingrese un numero entero.\n");

            limpiar_buffer();

            continue;

        }


        switch (opcion)
        {
        case INGRESAR_PRIMER_CRITERIO:
            ingresar_primer_criterio();
            break;

        case INGRESAR_SEGUNDO_CRITERIO:
            ingresar_segundo_criterio();
            break;

        case INGRESAR_TERCER_CRITERIO:
            ingresar_tercer_criterio();
            break;

        case BUSCAR:

            printf("\n");

            informacion_faltante = false;

            if(criterio_fecha[0] == '\0'){
                printf("Para realizar la busqueda, debe especificar una fecha.\n");
                informacion_faltante = true;
            }

            if(criterio_tiempo_inicio[0] == '\0'){
                printf("Para realizar la busqueda, debe especificar un tiempo inicial.\n");
                informacion_faltante = true;
            }

            if(criterio_tiempo_final[0] == '\0'){
                printf("Para realizar la busqueda, debe especificar un tiempo final.\n");
                informacion_faltante = true;
            }

            if(criterio_idioma[0] == '\0'){
                printf("Para realizar la busqueda, debe especificar un idioma.\n");
                informacion_faltante = true;
            }

            if(informacion_faltante){
                continue;
            }

            buscar();

            break;  

        case VER_CRITERIOS:
            ver_criterios();
            break;

        case SALIR:
            salir();
            break; 

        default:
            printf("Comando invalido.\n");
            break;
        }

    }
    
    return 0;
}