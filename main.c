#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "./stb_image_write.h"

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
	
//	#pragma omp parallel for simd
//	for (int i = 0; i < size/10; i++){
//		test_array[i] = 0.5;
//	}

	return test_array;
}

unsigned char* normalize_array(double * array, int array_size){
	int draw_size = array_size*3;
	unsigned char* drawable_array = malloc(sizeof(unsigned char)*draw_size);
	
	//there are absolutely optimizations that can be done here
	//like splitting the thing up and finding like 10 smaller mins and maxes, then combining
	//this may actually be a real performance improvement for large enough arrays
	double min = array[0];
	double max= array[0];

	for (int i = 0; i < array_size; i++){
		if (array[i] > max) {
			max = array[i];
		} else if (array[i] < min) {
			min = array[i];
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

		//#pragma omp parallel for
		//for (int i = 1; i < width*height-1; i ++) {
		//	array[i] = (0.8*temp[i] + 0.1*temp[i+1] + 0.1*temp[i-1]);
		//}
		
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

int main(int argc, char * argv[]) {
	//if (argc != 2) {
	//	printf("Usage: view <filename>\n");
	//}	

	int width, height, nrChannels;
	//stbi_set_flip_vertically_on_load(1);
	//unsigned char * data = stbi_load(argv[1], &width, &height, &nrChannels, 0);

	//if (!data) {
	//	printf("Invalid image.\n");
	//	exit(0);
	//}
	double * raw_data = generate_test_array();
	diffuse(raw_data,1920,1080,100);
	unsigned char* data = normalize_array(raw_data,1920*1080);
	stbi_write_jpg("test.jpg", 1920, 1080,3,data, 90);

	//time to get gl set up! 
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	GLFWwindow * window = glfwCreateWindow(1920,1080, "grid", NULL, NULL);

	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, window_size_callback);
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

	unsigned int VAO;
	glGenVertexArrays(1, &VAO);
	glBindVertexArray(VAO);

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

	unsigned int shader_program = glCreateProgram();
	glAttachShader(shader_program, fragment_shader);
	glAttachShader(shader_program, vertex_shader);
	glLinkProgram(shader_program);
	glGetProgramiv(shader_program, GL_LINK_STATUS, &success);
	assert(success);

	glUseProgram(shader_program);
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	glVertexAttribPointer(0,2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *) 0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void *)(2*sizeof(float)));
	glEnableVertexAttribArray(0);		
	glEnableVertexAttribArray(1);

	unsigned int texture; 
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	//modified for test array
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1920, 1080, 0, GL_RGB ,GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	
	//stbi_image_free(data);

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
	return 0;

}

