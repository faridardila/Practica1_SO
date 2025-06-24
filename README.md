
# Práctica I - Sistemas Operativos

| Nombre           | GitHub   | Contacto              |
| ---------------- | -------- |-------------------------|
| Deivid Farid Ardila Herrera | faridardila | deardilah@unal.edu.co |
| Cristofer Damián Camilo Ordoñez Osa | cristoferOrdonez| crordonezo@unal.edu.co |


## Tabla de Contenido
1. [Acerca del dataset](#Acerca-del-dataset)
2. [Acerca del programa](#Acerca-del-programa)
3. [Justificación del programa](#Justificación-del-programa)
4. [Ejemplos de uso](#Ejemplos-de-uso)

## Acerca del dataset

El *dataset* seleccionado, [*COVID-19 : Twitter Dataset Of 100+ Million Tweets*](https://www.kaggle.com/datasets/adarshsng/covid19-twitter-dataset-of-100-million-tweets?select=full_dataset_clean.tsv), recopila información de conversaciones publicadas en Twitter relacionadas con el COVID-19. Su propósito principal es apoyar investigaciones científicas. A continuación, se describen los datos contenidos en el conjunto datos:
| Campo           | Descripción                                        |
| :-------------- | :------------------------------------------------- | 
| **id** | Identificador unico para acceder a la información del tweet. | 
| **fecha** | Fecha de publicación del tweet (AAAA-MM-DD).    |
| **tiempo** | Hora exacta en la que fue publicado el tweet, expresada en formato de 24 horas (HH:MM:SS).       |
| **idioma** | Idioma en el que fue escrito el tweet (ej: en, es, ru). |
| **pais** | Lugar de proveniencia del tweet. (ej: PA, RU).               | 

**NOTA:** el *dataset* original esta en formato `.tsv`; sin embargo, para cumplir con los requerimientos de la práctica, se convirtió al formato `.csv`. La versión adaptada puede consultarse en el siguiente [*enlace*](https://drive.google.com/file/d/1LYDgP7psKjsBdR3FSvO-URfvH1k7OHWT/view?usp=sharing).

## Acerca del programa
### ¿Cómo ejecutar el programa?

Se ofrecen dos alternativas, según la conveniencia del usuario:

1. Ejecutando manualmente los comandos en consola:

    ```console
    make
    ./p2-dataProgram & ./p1-dataProgram
    make clean # limpia los ejecutables y archivos generados (opcional)
    ```
2. Mediante el script de bash (con permisos de ejecución previamente concedidos al usuario):

    ```console
    ./executeme.sh
    ```

De esta forma, el programa encargado de la indexación y comunicación con el *dataset* se ejecutará en segundo plano, mientras que la interfaz de usuario se mantendrá en primer plano.

###  Funciones Clave

Para el correcto funcionamiento del programa, se ejecutan dos procesos independientes (no emparentados) que se comunican entre sí.

El primer proceso, cuyo código fuente corresponde a `p1-dataProgram.c`, se encarga de la interacción con el usuario; entre sus funciones destaca:

* `buscar()`: esta función se comunica con el proceso de indexación y búsqueda para enviar los criterios de búsqueda definidos por el usuario. Luego, recibe los resultados correspondientes, los presenta organizados en forma de tabla, e informa tanto la cantidad total de registros encontrados como el tiempo que tomó obtenerlos.

  
El segundo proceso, implementado en el archivo `p2-dataProgram.c`, tiene como función principal la creación del archivo de indexación y la búsqueda de registros según los criterios recibidos. Sus funciones clave son:
    
* `crear_tabla_hash()`: construye una tabla hash almacenada en disco, indexando punteros que referencian directamente a los registros dentro del archivo `.csv`.
* `busqueda()`: realiza la búsqueda eficiente de registros en el archivo `.csv` que coincidan con los criterios proporcionados, utilizando como apoyo la tabla hash previamente generada.


## Justificación del programa y de las adaptaciones realizadas


### Campos de búsqueda

Para llevar a cabo la búsqueda de registros en el *dataset*, se seleccionaron los siguientes criterios:

| Criterio           | Justificación                                        | Rango     |
| :-------------- | :------------------------------------------------- | -------------|
| **fecha** | El filtrado por fecha permite analizar la evolución de las conversaciones sobre el COVID-19 a lo largo de distintos períodos relevantes, como el anuncio de la pandemia, la distribución de vacunas o la aparición de nuevas variantes. |Fechas entre 2019 y el año actual|
| **tiempo (inicial y final)** | El análisis temporal mediante el filtrado por rangos de hora permite detectar concentraciones de actividad en momentos específicos del día, evidenciadas por el volumen de tweets publicados. Esta información, combinada con el filtrado por fecha, resulta útil para identificar patrones de comportamiento durante períodos críticos de la pandemia.       | Horas entre 00:00:00 y 23:59:59. |
| **idioma** | El filtrado por idioma permite comprender los distintos contextos culturales y geográficos durante el desarrollo de la pandemia. A través de este análisis, es posible identificar percepciones, preocupaciones y posibles focos de desinformación presentes en diversas comunidades lingüísticas.  | Código alfabético de 2 o 3 letras. |


No se consideró el campo `pais`, ya que una gran parte de los registros carece de esta información, lo que limita su utilidad para fines de investigación o análisis de datos; tampoco se incluyó el campo `id` como criterio principal de búsqueda, dado que es poco probable que un usuario común disponga del identificador exacto de un tweet para realizar consultas específicas.

### Comunicación entre procesos

La implementación hace uso de memoria compartida y tuberías como mecanismos de comunicación entre procesos (IPC):

- **Memoria compartida:** este mecanismo es implementado para permitir que la interfaz de usuario transmita los criterios al proceso de busqueda y, a su vez, reciba los resultados obtenidos (registros correspondientes a tweets).

- **Tuberías nombradas (FIFO)**: este mecanismo se emplea para la señalización y el intercambio de información de control. Por ejemplo, se utiliza `testigo` para indicar el inicio de la búsqueda y se envía `cantidad_coincidencias` que comunica al proceso de interfaz de usuario el número de resultados obtenidos. 

### Indexación
Para asegurar una búsqueda menor a dos segundos, el sistema implementa una **tabla hash**.

* **Creación del Índice**: cuando el programa se ejecuta por primera vez (o si el archivo de índice aún no ha sido creado) se genera un archivo binario denominado `index.bin`. Este archivo contiene una tabla hash estructurada en dos componentes principales: la tabla de cabeceras y una serie de nodos de índice. Cada nodo incluye un `offset_csv`, que señala la posición exacta del registro en el archivo `.csv`, y un `offset_siguiente_nodo`, que permite gestionar colisiones mediante listas enlazadas implementadas dentro del mismo archivo binario `index.bin`.
  
* **Función Hash**: Se utiliza la función `hash_djb2` para realizar la indexación a partir de la fecha. Para manejar las colisiones, se utilizan listas enlazadas.
    * **¿Por qué el algoritmo djb2?**: El hash djb2 es un algoritmo de hashing desarrollado por Daniel J. Bernstein a mediados de la década de 1990. 
    
        Su construcción basada en operaciones de desplazamiento de bits y adición, permite que se ejecute muy rápidamente. Esto es crucial para el programa, ya que se necesita un rendimiento de búsqueda inferior a 2 segundos.
        
        Asimismo, la simplicidad de su algoritmo lo hace fácil de implementar en código, lo que facilitó el desarrollo del sistema de indexación.


* **Búsqueda**: Cuando se realiza una búsqueda, el proceso de búsqueda accede directamente a las posiciones en el archivo CSV a través de los offsets almacenados en el `index.bin`, lo que evita la necesidad de cargar todo el dataset en memoria.


## Ejemplos de uso 

Los ejemplos de uso se muestran a continuación:

- Ejemplo 1: 
    ```console
    Fecha: 2020-03-08

    Tiempo inicial: 00:00:00

    Tiempo final: 00:00:00

    Idioma: en
    ```

    Resultado: NA (no se encontraron coincidencias)

    Video: [Ejemplo 1](https://drive.google.com/file/d/1yF3abt5WhG6Ga5k4F-qZr0c7M6LSZc5M/view?usp=sharing)

- Ejemplo 2:

    ```console
    Fecha: 2021-01-01

    Tiempo inicial: 02:02:02

    Tiempo final: 02:02:02

    Idioma: en
    ```

    Resultado: 13 registros encontrados

    Video: [Ejemplo 2](https://drive.google.com/file/d/1-6B5TzvgpbSCA95nIgSeB-yKL_XSgTL-/view?usp=sharing)

- Ejemplo 3:

    ```console
    Fecha: 2020-01-01
    
    Tiempo inicial: 20:28:39
    
    Tiempo final: 20:28:39
    
    Idioma: ru
    ```

    Resultado: 1 registro encontrado

    Video: [Ejemplo 3](https://drive.google.com/file/d/15bwUQCBrp1Gmj93Isn6xhRJHGIDZckN_/view?usp=sharing)


