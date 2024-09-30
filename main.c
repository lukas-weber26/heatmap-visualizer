#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <pthread.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./stb_image_write.h"

void handle_close(GLFWwindow * window);
void window_size_callback(GLFWwindow * window, int xsize, int ysize);
double*generate_test_array();
unsigned char* normalize_array(double * array, int array_size);
void diffuse(double * array, int width, int height, int passes);

void handle_close(GLFWwindow * window) {
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
		glfwSetWindowShouldClose(window, 1);
	}
}

void window_size_callback(GLFWwindow * window, int xsize, int ysize){
	glViewport(0, 0, xsize, ysize);
}

double rand_from_range(double min, double max){
	double range = max-min;
	double div = RAND_MAX/range;
	return min+(rand()/div);
}

double*generate_test_array(){
	int size = 1920*1080;
	double *test_array = malloc(sizeof(double)*size);
	srand(time(NULL));
	
	#pragma omp parallel for simd
	for (int i = 0; i < size; i++){
		test_array[i] = pow(rand_from_range(-1.0, 1.0),2);
	}
	
	return test_array;
}

unsigned char* normalize_array(double * array, int array_size){
	int draw_size = array_size*3;
	unsigned char* drawable_array = malloc(sizeof(unsigned char)*draw_size);
	
	double min = array[0];
	double max= array[0];
	
	#pragma omp parallel
	{
		double local_min = array[0];
		double local_max= array[0];
		for (int i = 0; i < array_size; i++){
			if (array[i] > local_max) {
				local_max = array[i];
			} else if (array[i] < local_min) {
				local_min= array[i];
			}
			
		}
		#pragma omp critial 
		{
			if (local_min < min) {
				min = local_min;
			} else if (local_max > max) {
				max = local_max;
			}
		}
	}

	double range = (max - min);
	#pragma omp parallel for simd
	for (int i = 0; i < array_size; i ++) {
		unsigned char blue = (unsigned char)255*(array[i]- min)/range;
		drawable_array[3*i] = blue;	
		drawable_array[3*i+2] = 255-blue;	
	}
	
	return drawable_array;
}

void diffuse(double * array, int width, int height, int passes) {

	double * temp = malloc(sizeof(double)*width*height);

	for (int p = 0; p < passes; p++){
		
		memcpy(temp, array, sizeof(double)*width*height);

		#pragma omp parallel for
		for (int i = 1; i < height-1; i ++) {
			#pragma omp parallel for simd
			for (int j = 1; j < width-1; j++){
				array[j+width*i] = (0.6*temp[j+width*i] + 0.1*temp[j+width*i+1] + 0.1*temp[j+width*i-1] + 0.1*temp[j+(width)*(i-1)] + 0.1*temp[j+(width)*(i+1)]);
			}
		}
	}

	free(temp);

}

typedef struct drawstruct{
	const char * print_name;
	int width;
	int height;
	unsigned int shader_program;
	unsigned int texture; 
	unsigned int VAO;
	GLFWwindow* window;
	unsigned char * data;
} drawstruct;

void * initialize_glfw(void * arg) {

	drawstruct * drawable = (drawstruct *)(arg);

	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);	

	drawable->window = glfwCreateWindow(drawable->width,drawable->height, "grid", NULL, NULL);

	glfwMakeContextCurrent(drawable->window);
	glfwSetFramebufferSizeCallback(drawable->window, window_size_callback);
	glfwPollEvents();

	glewExperimental = GL_TRUE;
	glewInit();

	glViewport(0,0,1920, 1080);


	float verts [] = {
		-0.90, -0.90,0,0,
		0.90,-0.90,0.90,0,
		-0.90,0.90,0,0.90,
		0.90,-0.90,0.90,0,
		0.90,0.90,0.90,0.90,
		-0.90,0.90,0,0.90,
	};

	
	glGenVertexArrays(1, &drawable->VAO);
	glBindVertexArray(drawable->VAO);

	unsigned int VBO;
	glGenBuffers(1,&VBO);
	glBindBuffer(GL_ARRAY_BUFFER,VBO);
	glBufferData(GL_ARRAY_BUFFER,sizeof(verts), verts, GL_STATIC_DRAW);

	int success;
	const char * vertex_shader_source = "#version 330 core\n"
	"layout (location = 0) in vec2 aPos;\n"
	"layout (location = 1) in vec2 aTexCoord;\n"
	"out vec2 TexCoord;\n"
	"void main()\n"
	"{\n"
	"gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);\n"
	"TexCoord = aTexCoord;"
	"}\0";

	const char * fragment_shader_source = "#version 330 core\n"
	"out vec4 FragColor;\n"
	"in vec2 TexCoord;\n"
	"uniform sampler2D ourTexture;\n"
	"void main()\n"
	"{\n"
	"FragColor = texture(ourTexture,TexCoord);\n"
	"}\0";

	unsigned int vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
	glCompileShader(vertex_shader);
	glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
	assert(success);

	unsigned int fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
	glCompileShader(fragment_shader);
	glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);

	drawable->shader_program= glCreateProgram();
	glAttachShader(drawable->shader_program, fragment_shader);
	glAttachShader(drawable->shader_program, vertex_shader);
	glLinkProgram(drawable->shader_program);
	glGetProgramiv(drawable->shader_program, GL_LINK_STATUS, &success);
	assert(success);

	glUseProgram(drawable->shader_program);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	glVertexAttribPointer(0,2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *) 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *)(2*sizeof(float)));
	glEnableVertexAttribArray(0);		
	glEnableVertexAttribArray(1);

	glGenTextures(1, &drawable->texture);
	glBindTexture(GL_TEXTURE_2D, drawable->texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	return NULL;
}

void * display_image(void * arg){

	drawstruct * drawable = arg;
	int width = drawable->width;
	int height = drawable -> height;
	unsigned int shader_program = drawable->shader_program;
	unsigned int texture = drawable->texture; 
	unsigned int VAO = drawable->VAO;
	GLFWwindow * window = drawable ->window;
	unsigned char* data = drawable->data;
		
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB ,GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	while(!glfwWindowShouldClose(window)) {
		handle_close(window);
		glClearColor(1.0, 1.0, 1.0, 1.0);
		glClear(GL_COLOR_BUFFER_BIT);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture);

		glBindVertexArray(VAO);
		glUseProgram(shader_program);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glfwTerminate();

	return NULL;
}

void *print_image(void * arg){
	drawstruct * drawable = (drawstruct *)arg;
	int width = drawable->width;
	int height = drawable -> height;
	unsigned char *data = drawable->data;
	stbi_write_jpg(drawable->print_name, width, height,3,data, 90);
	return NULL;
}

void draw_heatmap(double * data, int width, int height, int free_data, int diffusion_steps, int show, int print, char * printname){

	pthread_t show_thread;
	pthread_t print_thread;
	drawstruct *drawable = malloc(sizeof(drawstruct));
	drawable->width = width;
	drawable->height = height;
	drawable->print_name = printname;
	
	if (show == 1){
		initialize_glfw(drawable);
	}

	if (free_data == 0) {
		double * temp_data = malloc(sizeof(double)*width*height);
		memcpy(temp_data, data, sizeof(double)*width*height);
		data = temp_data;
	}

	diffuse(data,width,height,diffusion_steps);
	unsigned char* normalized_data = normalize_array(data,width*height);
	
	if (print == 1) {
		drawable->print_name = printname;
		pthread_create(&print_thread, NULL, print_image, (void *)(&drawable));
		//print_image(drawable);
	}	

	if (show == 1){
		drawable->data = normalized_data;
		display_image(drawable);
	}

	if (print == 1) {
		//pthread_join(print_thread,NULL);
	}
	
	free(drawable);
	free(normalized_data);
	free(data);
}   

int main(){
	double * raw_data = generate_test_array();
	draw_heatmap(raw_data, 1920, 1080, 1, 5, 1, 1, "thread_test_print.jpg");
	//draw_heatmap(raw_data, 1920, 1080, 0, 0, 1, 1, "thread_test2_print.jpg");
}
