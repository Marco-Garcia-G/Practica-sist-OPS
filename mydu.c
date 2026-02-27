#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_PATH_LEN 4096
#define MAX_ENTRIES 1000

const char *binary_file = "mydu.bin";

typedef struct {
  long size_kb;
  char path[512];
} DirEntry;

long calculate_dir_size(const char *dirpath);

int write_binary_entry(int fd, long size_kb, const char *path) {
  DirEntry entry;

  entry.size_kb = size_kb;
  if (strlen(path) >= sizeof(entry.path)) {
    fprintf(stderr, "Error: ruta demasiado larga\n");
    return -1;
  }
  strcpy(entry.path, path);

  if (write(fd, &entry, sizeof(entry)) != sizeof(entry)) {
    return -1;
  }
  return 0;
}

long calculate_dir_size(const char *dirpath) {
  DIR *dir;
  struct dirent *entry;
  struct stat st;
  char fullpath[MAX_PATH_LEN];
  long total_size;
  long subdir_size;
  long subdir_kb;
  int fd;

  dir = opendir(dirpath);
  if (dir == NULL) {
    fprintf(stderr, "Error: no se pudo abrir directorio %s\n", dirpath);
    return -1;
  }

  total_size = 0;

  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    if (snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name) >= MAX_PATH_LEN) {
      fprintf(stderr, "Error: ruta demasiado larga\n");
      closedir(dir);
      return -1;
    }

    if (lstat(fullpath, &st) < 0) {
      fprintf(stderr, "Error: no se pudo acceder a %s\n", fullpath);
      closedir(dir);
      return -1;
    }

    if (S_ISDIR(st.st_mode)) {
      subdir_size = calculate_dir_size(fullpath);
      if (subdir_size < 0) {
        closedir(dir);
        return -1;
      }
      total_size += subdir_size;

      subdir_kb = (subdir_size + 511) / 512;
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

      printf("%ld\t%s\n", subdir_kb, fullpath);
    } else {
      total_size += st.st_size;
    }
  }

  closedir(dir);
  return total_size;
}

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
    nread = read(fd, &entry, sizeof(entry));
    if (nread < 0) {
      fprintf(stderr, "Error: no se pudo leer mydu.bin\n");
      close(fd);
      return -1;
    }
    if (nread == 0) {
      break;
    }
    if (nread != sizeof(entry)) {
      fprintf(stderr, "Error: entradas binarias corruptas\n");
      close(fd);
      return -1;
    }

    printf("%ld\t%s\n", entry.size_kb, entry.path);
  }

  close(fd);
  return 0;
}

int is_directory(const char *path) {
  struct stat st;

  if (lstat(path, &st) < 0) {
    return -1;
  }

  if (S_ISDIR(st.st_mode)) {
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  char *target_path;
  long total_size;
  long total_kb;
  int fd;

  (void)argc;
  (void)argv;

  if (argc == 1) {
    target_path = ".";
  } else if (argc == 2) {
    if (strcmp(argv[1], "-b") == 0) {
      return read_binary_history();
    }
    target_path = argv[1];
  } else {
    fprintf(stderr, "Uso: ./mydu [<directorio>]\n");
    fprintf(stderr, "Uso: ./mydu [-b]\n");
    return -1;
  }

  if (is_directory(target_path) != 1) {
    fprintf(stderr, "%s: No es un directorio\n", target_path);
    return -1;
  }

  total_size = calculate_dir_size(target_path);
  if (total_size < 0) {
    return -1;
  }

  total_kb = (total_size + 511) / 512;

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

  printf("%ld\t%s\n", total_kb, target_path);

  return 0;
}
