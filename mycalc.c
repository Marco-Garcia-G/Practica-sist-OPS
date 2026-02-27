#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

int to_int(const char *text, int *out) {
  char *end;
  long value;

  value = strtol(text, &end, 10);
  if (text[0] == '\0') {
    return -1;
  }
  if (*end != '\0') {
    return -1;
  }
  if (value < INT_MIN || value > INT_MAX) {
    return -1;
  }

  *out = (int)value;
  return 0;
}

void print_usage(void) {
  if (write(2, "Uso: ./mycalc <num1> <op> <num2>\n", 33) < 0) return;
  if (write(2, "Uso: ./mycalc -b <num_operacion>\n", 33) < 0) return;
}

void print_error(const char *msg, int len) {
  if (write(2, msg, (size_t)len) < 0) return;
}

int str_len(const char *text) {
  int i;
  i = 0;
  while (text[i] != '\0') {
    i++;
  }
  return i;
}

int int_to_text(int value, char *out) {
  int i;
  int j;
  unsigned long n;
  char tmp[20];

  if (value == 0) {
    out[0] = '0';
    return 1;
  }

  i = 0;
  j = 0;
  if (value < 0) {
    out[j] = '-';
    j++;
    n = (unsigned long)(-(value + 1)) + 1;
  } else {
    n = (unsigned long)value;
  }

  while (n > 0) {
    tmp[i] = (char)('0' + (n % 10));
    i++;
    n = n / 10;
  }

  while (i > 0) {
    i--;
    out[j] = tmp[i];
    j++;
  }

  return j;
}

int write_checked(int fd, const char *text, int len) {
  int total;
  ssize_t written;

  total = 0;
  while (total < len) {
    written = write(fd, text + total, (size_t)(len - total));
    if (written <= 0) {
      return -1;
    }
    total += (int)written;
  }
  return 0;
}

int write_operation(int fd, const char *num1, const char *op, const char *num2, const char *result_text) {
  if (write_checked(fd, "Operación: ", str_len("Operación: ")) < 0 ||
      write_checked(fd, num1, str_len(num1)) < 0 ||
      write_checked(fd, " ", 1) < 0 ||
      write_checked(fd, op, str_len(op)) < 0 ||
      write_checked(fd, " ", 1) < 0 ||
      write_checked(fd, num2, str_len(num2)) < 0 ||
      write_checked(fd, " = ", 3) < 0 ||
      write_checked(fd, result_text, str_len(result_text)) < 0 ||
      write_checked(fd, "\n", 1) < 0) {
    return -1;
  }
  return 0;
}

int write_history_line(int line_number, const char *line_text, int line_len) {
  char line_number_text[20];
  int line_number_len;

  line_number_len = int_to_text(line_number, line_number_text);
  line_number_text[line_number_len] = '\0';

  if (write_checked(1, "Linea ", 6) < 0 ||
      write_checked(1, line_number_text, line_number_len) < 0 ||
      write_checked(1, ": ", 2) < 0) {
    return -1;
  }

  if (write_checked(1, line_text, line_len) < 0) {
    return -1;
  }
  return 0;
}

int is_history_mode(const char *text) {
  if (text[0] == '-' && text[1] == 'b' && text[2] == '\0') {
    return 1;
  }
  return 0;
}

int compute_result(int a, int b, char op, int *out) {
  long tmp;

  if (op == '+') {
    tmp = (long)a + (long)b;
  } else if (op == '-') {
    tmp = (long)a - (long)b;
  } else if (op == '*' || op == 'x') {
    tmp = (long)a * (long)b;
  } else if (op == '/') {
    if (b == 0) {
      return -2;
    }
    if (a == INT_MIN && b == -1) {
      return -3;
    }
    tmp = a / b;
  } else {
    return -1;
  }

  if (tmp < INT_MIN || tmp > INT_MAX) {
    return -3;
  }

  *out = (int)tmp;
  return 0;
}

int main(int argc, char *argv[]) {
  int a;
  int b;
  int result;
  int line_number;
  int current_line;
  int fd;
  char op;
  char result_text[20];
  char line_buffer[512];
  char read_buffer[128];
  int pos;
  int result_len;
  int calc_status;
  int line_too_long;
  ssize_t nread;
  int i;

  if (argc == 3 && is_history_mode(argv[1]) == 1) {
    if (to_int(argv[2], &line_number) != 0 || line_number <= 0) {
      print_error("Error: numero de operacion invalido\n", 36);
      return -1;
    }

    fd = open("mycalc.log", O_RDONLY);
    if (fd < 0) {
      print_error("Error: no se pudo abrir mycalc.log\n", 35);
      return -1;
    }

    current_line = 1;
    pos = 0;
    line_too_long = 0;
    while (1) {
      nread = read(fd, read_buffer, sizeof(read_buffer));
      if (nread < 0) {
        close(fd);
        return -1;
      }
      if (nread == 0) {
        break;
      }

      for (i = 0; i < nread; i++) {
        if (current_line == line_number && pos < 511) {
          line_buffer[pos] = read_buffer[i];
          pos++;
        } else if (current_line == line_number && read_buffer[i] != '\n') {
          line_too_long = 1;
        }

        if (read_buffer[i] == '\n') {
          if (current_line == line_number) {
            close(fd);
            if (line_too_long == 1) {
              print_error("Error: linea de historial demasiado larga\n", 40);
              return -1;
            }
            if (write_history_line(line_number, line_buffer, pos) < 0) {
              return -1;
            }
            return 0;
          }
          current_line++;
          pos = 0;
        }
      }
    }

    if (current_line == line_number && pos > 0) {
      close(fd);
      if (line_too_long == 1) {
        print_error("Error: linea de historial demasiado larga\n", 40);
        return -1;
      }
      if (write_history_line(line_number, line_buffer, pos) < 0 ||
          write_checked(1, "\n", 1) < 0) {
        return -1;
      }
      return 0;
    }

    close(fd);
    print_error("Error: El numero de linea no es valido\n", 39);
    return -1;
  }

  if (argc != 4) {
    print_usage();
    return -1;
  }

  if (to_int(argv[1], &a) != 0 || to_int(argv[3], &b) != 0) {
    print_error("Error: numeros invalidos\n", 25);
    return -1;
  }

  if (argv[2][1] != '\0') {
    print_error("Error: operador invalido\n", 25);
    return -1;
  }

  op = argv[2][0];

  calc_status = compute_result(a, b, op, &result);
  if (calc_status == -1) {
    print_error("Error: operador invalido\n", 25);
    return -1;
  }
  if (calc_status == -2) {
    print_error("Error: Division por cero\n", 25);
    return -1;
  }
  if (calc_status == -3) {
    print_error("Error: overflow en operacion\n", str_len("Error: overflow en operacion\n"));
    return -1;
  }

  fd = open("mycalc.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    print_error("Error: no se pudo abrir mycalc.log\n", 35);
    return -1;
  }

  result_len = int_to_text(result, result_text);
  result_text[result_len] = '\0';

  if (write_operation(1, argv[1], argv[2], argv[3], result_text) < 0) {
    close(fd);
    return -1;
  }

  if (write_operation(fd, argv[1], argv[2], argv[3], result_text) < 0) {
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}
