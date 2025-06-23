
# Práctica I - Sistemas Operativos

## Acerca del dataset

El dataset elegido [*COVID-19 : Twitter Dataset Of 100+ Million Tweets*](https://www.kaggle.com/datasets/adarshsng/covid19-twitter-dataset-of-100-million-tweets?select=full_dataset_clean.tsv) contiene información de los tweets publicados con conversaciones relacionadas al COVID-19, con fines de investigación científica. Los datos que contiene el dataset se enuncian en la siguiente tabla:
| Campo           | Descripción                                        |
| :-------------- | :------------------------------------------------- | 
| **id** | El identificador para acceder a la información del tweet. | 
| **fecha** | La fecha de publicación del tweet (AAAA-MM-DD).    |
| **hora** | La hora de publicación del tweet (HH:MM:SS).       |
| **idioma** | El idioma en el que fue escrito el tweet (ej: en, es, ru). |
| **pais** | El lugar de proveniencia del tweet.                | 

## Acerca del programa

### Campos de búsqueda

Para el funcionamiento de la búsqueda de registros en el dataset, se escogieron los siguientes campos:

| Campo           | Justificación                                        | Rango     |
| :-------------- | :------------------------------------------------- | -------------|
| **fecha** | el filtrado por fecha permitiría analizar cómo las conversaciones del COVID-19 evolucionaron a lo largo de distintos periodos (anuncio de la pandemia, distribución de vacunas, aparición de nuevas variantes).    |Fechas entre 2019 y el año actual|
| **time** | el filtrado por tiempo permite detectar picos de tránsito en la aplicación por la cantidad de tweets publicados en horas específicas. Esto también está relacionado con la fecha, teniendo en cuenta los periodos clave durante la pandemia.       | Horas entre 00:00:00 y 23:00:00. |
| **lang** | el filtrado por idioma permite entender los diferentes contextos culturales y geográficos durante el tiempo de la pandemia. Se puede analizar la percepción, preocupaciones y desinformación de distintas comunidades linguísticas.  | Código de 2 o 3 letras (alfabético).  |


No se consideró `pais` , ya que hay una gran cantidad de registros a la que no se tiene esta información, por lo que no sería útil para fines de investigación o análisis de datos. Tampoco se consideró el `id` como campo principal,teniendo en cuenta que es simplemente un identificador.

### Comunicación entre procesos
Se utilizó memoria compartida y tuberías para diversas funcionalidades:

- **Memoria Compartida:** se implementó para comunicar los dos procesos para enviar los criterios de búsqueda (fecha, hora, idioma) y recibir resultados de la búsqueda (registros de `Tweet`) de la filtración al usuario.

- **Tuberías Nombradas (FIFOs)**: Se utilizan para la señalización y el envío de información de control, como el `testigo` para indicar que la búsqueda debe iniciar, y para enviar el `contador_coincidencias`, que indica el número de resultados debúsqueda al proceso de interfaz de usuario.

### Indexación y Búsqueda

Para asegurar una búsqueda menor a 2 segundos, el sistema implementa una **tabla hash**.

* **Creación del Índice**: Al iniciar el programa por primera vez (o si el archivo de índice no existe), se crea un archivo `index.bin`. Este archivo binario almacena la tabla de cabeceras de la tabla hash y los nodos del índice. Cada nodo contiene un offset que apunta al inicio de la línea correspondiente en el archivo CSV y un offset para manejar colisiones mediante listas enlazadas en el propio archivo `index.bin`.
* **Función Hash**: Se utiliza la función `hash_djb2` para realizar la indexación a partir de la fecha. Para manejar las colisiones, se utilizan listas enlazadas.
    * **¿Por qué el algoritmo djb2?**: 
* **Búsqueda**: Cuando se realiza una búsqueda, el proceso de búsqueda accede directamente a las posiciones en el archivo CSV a través de los offsets almacenados en el `index.bin`, lo que evita la necesidad de cargar todo el dataset en memoria.


## Ejemplos