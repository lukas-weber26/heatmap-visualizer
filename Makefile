visualizer: main.c
	cc main.c -O2 -o visualizer -fopenmp -lglfw -lGL -lX11 -lpthread -lXrandr -lXi -lm -ldl -lGLEW
