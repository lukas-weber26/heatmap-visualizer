visualizer: main.c
	cc main.c -g -o visualizer -fopenmp -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -lm -ldl -lGLEW
