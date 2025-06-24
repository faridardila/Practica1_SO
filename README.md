
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
| **hora** | Hora de publicación del tweet (HH:MM:SS).       |
| **idioma** | Idioma en el que fue escrito el tweet (ej: en, es, ru). |
| **pais** | Lugar de proveniencia del tweet. (ej: PA, RU).               | 

**NOTA:** el *dataset* original esta en formato `.tsv`; sin embargo, para cumplir con los requerimientos de la práctica, se convirtió al formato `.csv`. LLa versión adaptada puede consultarse en el siguiente [*enlace*](https://www.kaggle.com/datasets/adarshsng/covid19-twitter-dataset-of-100-million-tweets?select=full_dataset_clean.tsv).

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

El primer proceso, con su código fuente `p1-dataProgram.c` se ocupa de la interacción con el usuario.

* buscar(): función la cual se comunica con el proceso de indexación/búsqueda para enviar los criterios de búsqueda y recibir los resultados, para mostrarlos en una tabla, además de la cantidad de registros encontrados.

  
El primer proceso, con su código fuente `p2-dataProgram.c` se ocupa de la creación de la indexación y búsqueda de los registros de acuerdo con los criterios de búsqueda enviados por la interfaz de usuario.
    
* crear_tabla_hash(): función para crear la tabla hash en disco (indexando el apuntador al registro en el CSV)
* busqueda(): función para buscar los registros en el archivo CSV que tengan alguna coincidencia con los criterios de búsqueda, utilizando la tabla hash.


## Justificación del programa


### Campos de búsqueda

Para el funcionamiento de la búsqueda de registros en el dataset, se escogieron los siguientes campos:

| Campo           | Justificación                                        | Rango     |
| :-------------- | :------------------------------------------------- | -------------|
| **fecha** | el filtrado por fecha permitiría analizar cómo las conversaciones del COVID-19 evolucionaron a lo largo de distintos periodos (anuncio de la pandemia, distribución de vacunas, aparición de nuevas variantes).    |Fechas entre 2019 y el año actual|
| **tiempo (inicial y final)** | el filtrado por tiempo permite detectar picos de tránsito en la aplicación por la cantidad de tweets publicados en horas específicas. Esto también está relacionado con la fecha, teniendo en cuenta los periodos clave durante la pandemia.       | Horas entre 00:00:00 y 23:59:59. |
| **idioma** | el filtrado por idioma permite entender los diferentes contextos culturales y geográficos durante el tiempo de la pandemia. Se puede analizar la percepción, preocupaciones y desinformación de distintas comunidades linguísticas.  | Código de 2 ó 3 letras (alfabético).  |


No se consideró `pais` , ya que hay una gran cantidad de registros a la que no se tiene esta información, por lo que no sería útil para fines de investigación o análisis de datos. Tampoco se consideró el `id` como campo principal, teniendo en cuenta que no es probable que un usuario común tenga la información del identificador de un tweet para realizar una búsqueda.

### Comunicación entre procesos
Se utilizó memoria compartida y tuberías para diversas funcionalidades:

- **Memoria Compartida:** se implementó para comunicar los dos procesos para que la interfaz de usuario reciba los resultados de la búsqueda (registros de `Tweet`).

- **Tuberías Nombradas (FIFOs)**: Se utilizan para la señalización y el envío de información de control, como el `testigo` para indicar que la búsqueda debe iniciar, y para enviar el `contador_coincidencias`, que indica el número de resultados debúsqueda al proceso de interfaz de usuario. Además, se envía la información de los criterios de búsqueda desde la interfaz de usuario al proceso de indexación/búsqueda.

### Indexación
Para asegurar una búsqueda menor a 2 segundos, el sistema implementa una **tabla hash**.

* **Creación del Índice**: Al iniciar el programa por primera vez (o si el archivo de índice no existe), se crea un archivo `index.bin`. Este archivo binario almacena la tabla de cabeceras de la tabla hash y los nodos del índice. Cada nodo contiene `offset_csv` que apunta al inicio de la línea correspondiente en el archivo CSV y `offset_siguiente_nodo` que apunta al siguiente nodo para manejar colisiones mediante listas enlazadas en el propio archivo `index.bin`.
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


