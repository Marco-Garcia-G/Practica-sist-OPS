/*
mycalc.c - Calculadora con historial de operaciones

Este programa funciona como una calculadora simple desde la terminal.
No se han utilizaso librearia stdio.h, sino directamente con llamadas al sistema 

Modos de uso:
  -Calculadora: ./mycalc <num1> <op> <num2>
  -Historial: ./mycalc -b <num_operacion>
*/

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

/*
to_int: convierte un texto como "123" a un entero 123. Si el texto no es un numero valido, devuelve -1.
Hemos utilizado strtol para verificar que es un numero valido.
*/
int to_int(const char *text, int *out) {
  char *end; /*apunta al primer caracter que no es un digito */
  long value; /*strtol devuelve un long y luego lo convertimos a int*/

  value = strtol(text, &end, 10); /*convertimos en base 10*/
  /*si el texto esta vacio, no hay que convertir nada*/
  if (text[0] == '\0') {
    return -1;
  }
  /*si 'end' no llega al final, habia algo que no era nuemero (ej:"5abc34")*/
  if (*end != '\0') {
    return -1;
  }
  /*int tiene limintes si son nuemeros muy grandes*/ 
  if (value < INT_MIN || value > INT_MAX) {
    return -1;
  }

  *out = (int)value;
  return 0;
}

/*
print_usage: muestra como se usa el programa cuando el ussuario lo llama mal.
Escribimos siempre en fd=2, que es stderr (la salida de error estandar).
*/
void print_usage(void) {
  if (write(2, "Uso: ./mycalc <num1> <op> <num2>\n", 33) < 0) return;
  if (write(2, "Uso: ./mycalc -b <num_operacion>\n", 33) < 0) return;
}

/*
print_error: escribe un mensaje de error por stderr
*/
void print_error(const char *msg, int len) {
  if (write(2, msg, (size_t)len) < 0) return;
}

/*
str_len: calcula cuantos bytes tiene un texto
Recorremos hasta encontrar el caracter nulo '\0' que marca el final y contamos los caracteres
*/
int str_len(const char *text) {
  int i;
  i = 0;
  while (text[i] != '\0') {
    i++;
  }
  return i;
}

/*
int_to_text:convierte un numero entero a su representacion en texto.

La idea es dividir el numero por 10 sucesivamente para obtener cada digito, y guardarlos en un buffer temporal.
Luego los copiamos al buffer de salida en el orden correcto. 

*/
int int_to_text(int value, char *out) {
  int i;
  int j;
  unsigned long n;
  char tmp[20]; /*buffer temporal para almacenar los digitos*/
  /*caso especial: 0*/
  if (value == 0) {
    out[0] = '0';
    return 1;
  }

  i = 0;
  j = 0;
  if (value < 0) {
    out[j] = '-'; /*escribimos el signo negativo*/
    j++;
    /*evitamos problemas con INT_MIN*/
    n = (unsigned long)(-(value + 1)) + 1;
  } else {
    n = (unsigned long)value;
  }
/*extraemos digitos de derecha a izquierda y lo guardamos en tmp*/
  while (n > 0) {
    tmp[i] = (char)('0' + (n % 10)); /*convertimos el digito a su caracter ASCII*/
    i++;
    n = n / 10;
  }
/*copiamos los digitos en el orden corrrecto*/
  while (i > 0) {
    i--;
    out[j] = tmp[i];
    j++;
  }

  return j;
}

/*
write_checked: escribe un texto en un fd asegurandose de que se escriba todo. Si hay un error, devuelve -1.
El problema es que la llamada a write puede escribir menos bytes de los que le pedimos, quedando el texto cortado.
Para solucionarlo, hacemos un bucle que sigue llamando a write hasta que se haya escrito todo el texto.
*/
int write_checked(int fd, const char *text, int len) {
  int total;
  ssize_t written;

  total = 0;
  while (total < len) {
    /*escribimos donde nos qudamos la anterior vez*/
    written = write(fd, text + total, (size_t)(len - total));
    if (written <= 0) {
      return -1;
    }
    total += (int)written;
  }
  return 0;
}

/*
write_operation: construye y escribe la operación completa
Escribe en el formato: "Operación: num1 op num2 = result\n"
Se utiliza dos veces: una para escribir en la salida estandar y otra para escribir en el log.
*/
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

/*
write_history_line: escribe una linea del historial con su numero.
Escribe en el formato: "Linea N: contenido de la linea\n"
Convertimos el numero de linea a texto con int_to_text y luego escribimos todo el mensaje utilizando write_checked.
*/
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

/*
is_history_mode: verifica si el programa se ha llamado en modo historial.
El argumento tiene que ser exactamente "-b" para que se considere modo historial. Si es así, devuelve 1, sino devuelve 0.
*/
int is_history_mode(const char *text) {
  if (text[0] == '-' && text[1] == 'b' && text[2] == '\0') {
    return 1;
  }
  return 0;
}

/*
compute_result: realiza la operación entre dos numeros y devuelve el resultado.

Usamomos long para el resultado intermedio para detectar posibles overflow.

Aceptamos tanto '*' como 'x' para la multiplicacion, aunque la practica pide 'x', es comodo tener los dos.

Codigos de retorno:
  0: operacion exitosa, el resultado se guarda en *out
  -1: operador invalido
  -2: division por cero
  -3: overflow en la operacion
*/
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
      return -2; /*No se puede dividir entre cero*/
    }
    if (a == INT_MIN && b == -1) {
      return -3; /* caso que desbordaria*/
    }
    tmp = a / b;
  } else {
    return -1; /*operador invalido*/
  }
/*verificamos que el resultado no desborde los limites de int*/
  if (tmp < INT_MIN || tmp > INT_MAX) {
    return -3;
  }

  *out = (int)tmp;
  return 0;
}

/*
main: punto de entrada y logica principal del programa.

Lo primero es mirar los argumentos para ver el modo que estamos.
Si el el primer argumento es '-b' y hay 3 argumentos, estamos en modo historial.
Si hay 4 argumentos, estamos en modo calculadora.
Cualquier otra combinacion de argumentos es invalida y mostramos el mensaje de uso.
*/
int main(int argc, char *argv[]) {
  int a;
  int b;
  int result;
  int line_number;
  int current_line;
  int fd;
  char op;
  char result_text[20];
  char line_buffer[512];    /*acumulamos la linea que estamos buscando*/
  char read_buffer[128];    /*Leemos el fichero en bloque de 128 bytes*/
  int pos;
  int result_len;
  int calc_status;
  int line_too_long;
  ssize_t nread;
  int i;

  /*MODO HISTOSIAL: ./ mycalc -b <numero_de_linea> */
  if (argc == 3 && is_history_mode(argv[1]) == 1) {
    /*el numero de linea tiene que ser un entero positivo*/
    if (to_int(argv[2], &line_number) != 0 || line_number <= 0) {
      print_error("Error: numero de operacion invalido\n", 36);
      return -1;
    }
    /*abrimos el log solo para leer*/
    fd = open("mycalc.log", O_RDONLY);
    if (fd < 0) {
      print_error("Error: no se pudo abrir mycalc.log\n", 35);
      return -1;
    }

    current_line = 1; 
    pos = 0;
    line_too_long = 0;
    /*leemos el fichero en bloques de 128 bytes hasta encontrar la linea que queremos o llegar al final del fichero*/
    while (1) {
      nread = read(fd, read_buffer, sizeof(read_buffer));
      if (nread < 0) {
        close(fd);
        return -1;
      }
      if (nread == 0) {
        break; /*llegamos al final del fichero*/
      }

      for (i = 0; i < nread; i++) {
        /*si es la linea buscada, acumulamos el carecter */
        if (current_line == line_number && pos < 511) {
          line_buffer[pos] = read_buffer[i];
          pos++;
        } else if (current_line == line_number && read_buffer[i] != '\n') {
          line_too_long = 1;
        }
        /*cuando encontramos '\n' hemos teminado la linea actual*/
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
          /*no era la linea buscada, avanzamos a la siguiente*/
          current_line++;
          pos = 0;
        }
      }
    }
    /*llegamos al final del fichero sin encontrar la linea buscada.*/
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
    /*si llegamos aqui, la linea pedido no existe en el fichero*/
    close(fd);
    print_error("Error: El numero de linea no es valido\n", 39);
    return -1;
  }

  /*MODO CALCULADORA: ./mycalc <num1> <op> <num2>*/

  if (argc != 4) {
    print_usage();
    return -1;
  }
  /*convertimos los argumentos de texto a entero*/
  if (to_int(argv[1], &a) != 0 || to_int(argv[3], &b) != 0) {
    print_error("Error: numeros invalidos\n", 25);
    return -1;
  }
  /* el operador tiene que ser exactamente un caracter, no mas */
  if (argv[2][1] != '\0') {
    print_error("Error: operador invalido\n", 25);
    return -1;
  }

  op = argv[2][0];
  /* calculamos y gestionamos cada tipo de error posible */
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
  
  /*
  abrimos el log para añadir la operación al final
  usamos O_APPEND para no borrar el contenido existente. O_CREAT para crear el archivo si no existe 
  */
  fd = open("mycalc.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd < 0) {
    print_error("Error: no se pudo abrir mycalc.log\n", 35);
    return -1;
  }

  result_len = int_to_text(result, result_text);
  result_text[result_len] = '\0';
  /* primero mostramos la operacion por pantalla (fd=1 es stdout) */
  if (write_operation(1, argv[1], argv[2], argv[3], result_text) < 0) {
    close(fd);
    return -1;
  }
  /* luego guardamos exactamente la misma linea en el fichero de log */
  if (write_operation(fd, argv[1], argv[2], argv[3], result_text) < 0) {
    close(fd);
    return -1;
  }

  close(fd);
  return 0;
}
