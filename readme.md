#------ Proyecto 1 - Sistemas Operativos en Tiempo Real ------#
Para compilar, correr el comando     make

Para correr el proyecto, utilizar el comando     ./Proyecto1

El ejecutable utiliza el archivo config.txt para cargar las configuraciones del scheduler
El archivo de configuracion reside en el mismo directorio que el ejecutable y el .c
ATENCION: El uso inadecuado del archivo de configuracion resulta en comportamiento indefinido, muy
	posiblemente colapso del programa. Debe proveerse siempre todos los argumentos.
El formato del config.txt esta dado como sigue:

----------------------------------ejemplo de archivo de texto-------------------------------------
# 0 is RR, 1 is Lottery
algorithm=1
numProc=7
arrTime=2 4 5 7 8 56 70
procWork=2000000 1000000 500000 700000 750000 600000 1500000
ticketNum=10 20 30 50 60 70 80
quantum=200
---------------------------------------fin del ejemplo--------------------------------------------

Lineas de comentario comienzan con '#'

Descripcion de parametros
	- algorithm:
		Puede tomar unicamente el valor de 0 o 1, siendo '0' Round Robin, y '1' Lottery Scheduling
	- numProc:
		Numero de procesos o tareas a simular
	- arrTime:
		lista de [numProc] numeros enteros positivos (tambien el cero), separados por un espacio,
		correspondientes al tiempo de llegada de cada proceso
	- procWork:
		lista de [numProc] numeros enteros mayores a cero, separados entre ellos por un espacio,
		los cuales se habran de multiplicar por 50, el resultado de ello corresponde a la cantidad
		de iteraciones que se utilizaran en el calculo de PI, para cada proceso
	- ticketNum:
		lista de [numProc] numeros enteros, separados entre ellos por un espacio, correspondientes
		a la cantidad de tiquetes de cada proceso, para el Lottery Scheduling
	- quantum:
		numero entero mayor a cero, correspondiente al tiempo en uS a utilizar para el quantum del
		"procesador"