/*
mydu.c - Calculador del tamaño de directorios con historial 

Este programa recorre un directorio de forma recursiva y calcula cuanto espacio
ocupa en KB. Ademas guarda cada resultado en un fichero binario "mydu.bin" para poder consultarlo posteriormente.

Aqui hemos utilizado printf y perror para mostrar cosas por pantalla. 

Modos de uso:
./mydu : analiza el directorio actual
./mydu <directorio> : analiza el directorio especificado
./mydu -b : muestra el contenido del historial guardado en mydu.bin
*/
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
/* longitud maxima que puede tener una ruta en el sistema */
#define MAX_PATH_LEN 4096
#define MAX_ENTRIES 1000
/* nombre fijo del fichero binario donde guardamos el historial */
const char *binary_file = "mydu.bin";

/*
 DirEntry: estructura que define como guardamos cada entrada en el fichero binario

Decidimos usar una estrucructura con tamaño fijo para facilitar la lectura y escritura en el 
fichero binario. Cada entrada tiene un campo de tamaño en KB y otro campo con la ruta del directorio.
 */
typedef struct {
  long size_kb; /* tamano del directorio en kilobytes */
  char path[512]; /* ruta del directorio (maximo 512 caracteres) */
} DirEntry;

long calculate_dir_size(const char *dirpath);
/*
 write_binary_entry: guarda una entrada en el fichero binario

 Rellenamos la estructura DirEntry y la escribimos de golpe con write().
 Al escribir la estructura entera de una vez nos aseguramos de que
 el fichero tiene siempre entradas de tamano fijo, lo que facilita
 leerlas despues en read_binary_history().
 Devuelve 0 si fue bien, -1 si hubo algun error.
 */
int write_binary_entry(int fd, long size_kb, const char *path) {
  DirEntry entry;

  entry.size_kb = size_kb;
  /* nos aseguramos de que la ruta no es mas larga de lo que cabe en el campo */
  if (strlen(path) >= sizeof(entry.path)) {
    fprintf(stderr, "Error: ruta demasiado larga\n");
    return -1;
  }
  strcpy(entry.path, path);
  /* write devuelve cuantos bytes escribio. Si no son exactamente sizeof(entry), algo fallo */
  if (write(fd, &entry, sizeof(entry)) != sizeof(entry)) {
    return -1;
  }
  return 0;
}

/*
 calculate_dir_size: calcula el tamano total de un directorio de forma recursiva
 
 Esta es la funcion mas importante del programa. La idea es:
 1. Abrir el directorio y leer sus entradas una a una
 2. Para cada entrada que sea un subdirectorio, llamarnos a nosotros mismos
 recursividad) para calcular su tamano y sumarlo al total
 3. Para cada fichero regular, sumar directamente su tamano en bytes
 
 Usamos lstat() en lugar de stat() porque si hubiera enlaces simbolicos,
 stat() los seguiria y podriamos entrar en un bucle infinito. lstat()
 nos da informacion del enlace en si, no de donde apunta.
 
 Devuelve el tamano total en BYTES (la conversion a KB la hacemos fuera).
 Devuelve -1 si ocurre algun error.
 */
long calculate_dir_size(const char *dirpath) {
  DIR *dir;
  struct dirent *entry;
  struct stat st;
  char fullpath[MAX_PATH_LEN];
  long total_size;
  long subdir_size;
  long subdir_kb;
  int fd;
  /* intentamos abrir el directorio para poder leer sus entradas */
  dir = opendir(dirpath);
  if (dir == NULL) {
    fprintf(stderr, "Error: no se pudo abrir directorio %s\n", dirpath);
    return -1;
  }

  total_size = 0;

  /*
   readdir nos va dando las entradas del directorio una a una.
   Cuando no quedan mas devuelve NULL y salimos del bucle.
   */
  while ((entry = readdir(dir)) != NULL) {
    /*
     Todos los directorios tienen dos entradas especiales:
      "."  apunta al propio directorio (a si mismo)
      ".." apunta al directorio padre
     Si no las saltamos, la recursion no terminaria nunca.
     */
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

      /*
     Construimos la ruta completa uniendo el directorio padre con el nombre de la entrada. Por ejemplo: "testdir" + "subdir1"
     se convierte en "testdir/subdir1". snprintf nos protege de desbordamientos limitando la longitud.
     */
    if (snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name) >= MAX_PATH_LEN) {
      fprintf(stderr, "Error: ruta demasiado larga\n");
      closedir(dir);
      return -1;
    }
    /* obtenemos informacion sobre este fichero o directorio */
    if (lstat(fullpath, &st) < 0) {
      fprintf(stderr, "Error: no se pudo acceder a %s\n", fullpath);
      closedir(dir);
      return -1;
    }

    if (S_ISDIR(st.st_mode)) {
      /*
       Es un subdirectorio: nos llamamos a nosotros mismos con su ruta.
       El resultado es el tamano de ese subdirectorio en bytes, que sumamos al total del directorio padre.
       */
      subdir_size = calculate_dir_size(fullpath);
      if (subdir_size < 0) {
        closedir(dir);
        return -1;
      }
      total_size += subdir_size;
        /*
       Convertimos a KB antes de guardar en el binario.
       Sumamos 511 antes de dividir para redondear hacia arriba:
       si un directorio ocupa 513 bytes, ocupa 2 bloques de 512, no 1.
       Esto imita el comportamiento del comando 'du' del sistema.
       */
      subdir_kb = (subdir_size + 511) / 512;
      /* guardamos esta entrada en el fichero binario */
      fd = open(binary_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (fd < 0) {
        fprintf(stderr, "Error: no se pudo abrir mydu.bin\n");
        closedir(dir);
        return -1;
      }
      if (write_binary_entry(fd, subdir_kb, fullpath) < 0) {
        close(fd);
        closedir(dir);
        return -1;
      }
      close(fd);
      /* mostramos el subdirectorio y su tamano por pantalla */
      printf("%ld\t%s\n", subdir_kb, fullpath);
    } else {
      /* Es un fichero regular: sumamos su tamano en bytes al total */
       total_size += st.st_size;
      total_size += st.st_size;
    }
  }

  closedir(dir);
  return total_size;
}

/*
 read_binary_history: lee el fichero binario y muestra su contenido
 
 Aprovechamos que las entradas tienen tamano fijo (sizeof(DirEntry))
 para leer el fichero de forma muy sencilla: leemos bloques del
 tamano exacto de la estructura en un bucle hasta llegar al final.
 
 Si read() devuelve 0 es que llegamos al fin del fichero (normal).
 Si devuelve un numero distinto de sizeof(DirEntry) es que el
 fichero esta corrupto o se escribio mal.
 */
int read_binary_history(void) {
  int fd;
  DirEntry entry;
  ssize_t nread;

  fd = open(binary_file, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "Error: no se pudo abrir mydu.bin\n");
    return -1;
  }

  printf("--- Contenido del archivo binario ---\n");

  while (1) {
    /* intentamos leer exactamente una estructura entera */
    nread = read(fd, &entry, sizeof(entry));
    if (nread < 0) {
      fprintf(stderr, "Error: no se pudo leer mydu.bin\n");
      close(fd);
      return -1;
    }
    if (nread == 0) {
      break; /* fin de fichero, hemos leido todas las entradas */
    }
    if (nread != sizeof(entry)) {
      /* leimos menos de lo esperado: el fichero no esta bien formado */
      fprintf(stderr, "Error: entradas binarias corruptas\n");
      close(fd);
      return -1;
    }

    /* mostramos la entrada en formato legible */
    printf("%ld\t%s\n", entry.size_kb, entry.path);
  }

  close(fd);
  return 0;
}

/*
is_directory: comprueba si una ruta apunta a un directorio

 Usamos lstat() para obtener la informacion y luego la macro S_ISDIR() para comprobar si el campo st_mode indica que es un directorio.
 Esta funcion la necesitamos para rechazar ficheros como argumento, ya que el enunciado dice que solo se aceptan directorios.
 
 Devuelve: 1 si es directorio, 0 si existe pero no es directorio, -1 si error
 */
int is_directory(const char *path) {
  struct stat st;

  if (lstat(path, &st) < 0) {
    return -1; /* no se pudo acceder: no existe o sin permisos */
  }

  if (S_ISDIR(st.st_mode)) {
    return 1;
  }
  return 0; /* existe pero es un fichero u otro tipo */
}

/*
 main: decide que hace el programa segun los argumentos recibidos
 
 La logica de argumentos es sencilla:
 - Sin argumentos: analizamos "." (el directorio actual)
 - Con "-b": mostramos el historial del binario y terminamos
 - Con un directorio: lo analizamos
 - Con mas de un argumento o un fichero: error
 
 Tras calcular el tamano, guardamos el directorio raiz en mydu.bin
 y lo mostramos por pantalla como ultima linea de la salida.
 */
int main(int argc, char *argv[]) {
  char *target_path;
  long total_size;
  long total_kb;
  int fd;

  (void)argc;
  (void)argv;
  /* determinamos el directorio objetivo segun los argumentos */
  if (argc == 1) {
    /* sin argumentos analizamos el directorio donde estamos */
    target_path = ".";
  } else if (argc == 2) {
    if (strcmp(argv[1], "-b") == 0) {
      /* modo lectura del historial: solo leemos y mostramos el binario */
      return read_binary_history();
    }
    target_path = argv[1];
  } else {
    /* demasiados argumentos, mostramos como se usa */
    fprintf(stderr, "Uso: ./mydu [<directorio>]\n");
    fprintf(stderr, "Uso: ./mydu [-b]\n");
    return -1;
  }

    /*
   Comprobamos que el argumento sea un directorio.
   Si alguien hace ./mydu mycalc.c no tiene sentido y lo rechazamos.
   El enunciado dice que en ese caso mostramos el nombre y un error.
   */

  if (is_directory(target_path) != 1) {
    fprintf(stderr, "%s: No es un directorio\n", target_path);
    return -1;
  }

  /* calculamos el tamano total del directorio de forma recursiva */
  total_size = calculate_dir_size(target_path);
  if (total_size < 0) {
    return -1;
  }
  /* convertimos a KB redondeando hacia arriba */
  total_kb = (total_size + 511) / 512;

  /* guardamos el resultado en el fichero binario */
  fd = open(binary_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    fprintf(stderr, "Error: no se pudo abrir mydu.bin\n");
    return -1;
  }
  if (write_binary_entry(fd, total_kb, target_path) < 0) {
    close(fd);
    return -1;
  }
  close(fd);
  /* mostramos el tamaño total del directorio raiz como ultima linea */
  printf("%ld\t%s\n", total_kb, target_path);

  return 0;
}
