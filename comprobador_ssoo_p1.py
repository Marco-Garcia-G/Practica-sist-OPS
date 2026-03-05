'''
Sistemas Operativos - Practica 1 - Llamadas al sistema

Este programa verifica que el formato del entregable de la practica es el correcto (sigue las especificaciones de nombrado, y esta bien comprimido).

'''
import subprocess
import os
import sys
import shutil

def print_format():
	print("Formato esperado para zip: ssoo_p1_AAAAA_BBBBB_CCCCC.zip")
	print("Formato esperado para autores.txt: NIA1,Apellidos,Nombre(s)")


if(__name__=="__main__"):
    '''
    Funcion main de la aplicacion. Obtiene el fichero pasado como argumento. Lo descomprime, chequea su formato y finalmente lanza las pruebas.
    '''

    if shutil.which("unzip") is None:
        print("Error: El comando 'unzip' no está instalado en el sistema.")
        sys.exit(1)

    #Comprobamos que se ha pasado un fichero como argumento
    if not len(sys.argv) == 2:
        print('Uso: python comprobador_ssoo.py <fichero zip>')
        print_format()
    else:
        print('CORRECTOR: corrigiendo', sys.argv[1])
        inputFile = sys.argv[1]
        
        #Comprobamos que el fichero existe
        if not os.path.isfile(inputFile):
            print("El fichero", inputFile, "no existe")
            sys.exit(0)
    
        #Comprobamos el formato del nombre del fichero
        tokens=inputFile.replace(".zip","")
        tokens=tokens.split("_")
        if len(tokens) != 3 and len(tokens) != 4 and len(tokens) != 5:
            print("Formato del nombre del archivo incorrecto")
            print_format()
            sys.exit(0)
            
        ssoo=tokens[0]
        p1=tokens[1]
        u1=tokens[2]
        u2=""
        u3=""
        if len(tokens)>3:
            u2=tokens[3]
        if len(tokens)>4:
            u3=tokens[4]
        if not (ssoo == "ssoo" and p1 == "p1"):
            print("Formato del nombre del archivo incorrecto")
            print_format()
            sys.exit(0)

        print("CORRECTOR: NIA 1:",u1, "NIA 2:", u2, "NIA 3:", u3)

        print('CORRECTOR: descomprimiendo')

        #Descomprimimos el fichero en la carpeta temporal
        subprocess.call(["mkdir", "-p", "./check"])
        tempFolder="./check"
        zipresult=subprocess.call(["unzip","-j",inputFile,"-d",tempFolder])
        if not zipresult == 0:
            print("Error al descomprimir el fichero zip")
            sys.exit(0)

        #Comprobamos que el fichero de autores existe
        autoresPath = tempFolder+"/autores.txt"
        if not os.path.isfile(autoresPath):
            print("El fichero autores.txt no existe en la carpeta descomprimida")
            sys.exit(0)
    
        print("CORRECTOR: verificando formato de autores.txt")
        try:
            with open(autoresPath, "r", encoding="utf-8", errors="ignore") as f:
                lines = f.readlines()
                
                if not lines:
                    print("Error: autores.txt está vacío")
                    sys.exit(0)

                for linea_num, line in enumerate(lines, 1):
                    line = line.strip()
                    if not line: 
                        continue 

                    parts = line.split(",")
                    print("Línea {} {}".format(linea_num, line))
                    
                    if len(parts) != 3:
                        print(f"Error formato autores.txt (Línea {linea_num}): '{line}'")
                        print_format()
                        sys.exit(0)
                    
                    nia, apellidos, nombre = parts[0].strip(), parts[1].strip(), parts[2].strip()

                    # Verificar que el NIA es un número
                    if not nia.isdigit():
                        print(f"Error formato autores.txt (Línea {linea_num}): El NIA '{nia}' no es válido.")
                        print_format()
                        sys.exit(0)

                    # Verificar que apellidos y nombre tengan contenido
                    if not apellidos or not nombre:
                        print(f"Error formato autores.txt (Línea {linea_num}): Faltan apellidos o nombre.")
                        print_format()
                        sys.exit(0)

        except Exception as e:
            print(f"Error leyendo autores.txt: {e}")
            sys.exit(0)

        # Compilar
        makeres=subprocess.call(["make"], cwd=tempFolder)
        #makeres=subprocess.call(["make"])
        if not makeres == 0:
            print("Error de compilación")
            sys.exit(0)
        else:
            print("Compilación correcta")