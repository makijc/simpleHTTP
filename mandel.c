/// 
//  mandel.c
//  Based on example code found here:
//  https://users.cs.fiu.edu/~cpoellab/teaching/cop4610_fall22/project3.html
//
//  Converted to use jpg instead of BMP and other minor changes
//  
///
#include <sys/types.h> // for wait, etc.
#include <sys/wait.h> // for wait, etc.
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "jpegrw.h"

//image struct
typedef struct {
    imgRawImage *img;
	int height;
	int width;
    double xmin;
    double xmax;
    double ymin;
	double ymax;
	int max;
	int num_threads;
	int thread_height_min;
	int thread_height_max;
} image_parameters;

// local routines
static int iteration_to_color( int i, int max );
static int iterations_at_point( double x, double y, int max );
static void compute_image( imgRawImage *img, double xmin, double xmax,
					double ymin, double ymax, int max, int num_threads);
static void show_help();
void child_process(sem_t* sem1, char outfile[50], char name[15], int i,
	double xscale, double yscale, double xcenter, double ycenter,
			int image_width, int image_height, int max, int num_threads);
void* threadmethod(void*);


int mandelmain(double x_center, double y_center, double scale, int max_iter, int img_num)
{
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.
	char name[15] = "mandel";
	char outfile[50];
	double xcenter = 0;
	double ycenter = 0;
	double xscale = 4;
	double yscale = 0; // calc later
	int    image_width = 1000;
	int    image_height = 1000;
	int    max = 1000;
	int    image_num = 1;
	int    conc_children = 5;
	int    num_threads = 4;

	//replace defaults when necessary
	xcenter = x_center;
	ycenter = y_center;
	if(scale > 0){
		xscale = scale;
	}
	if(max_iter > 0){
		max = max_iter;
	}
	if(img_num > 0){
		image_num = img_num;
	}

	//Declare Semaphore
	sem_t* sem1 = sem_open("/lotsofkids",O_CREAT,S_IRWXU,conc_children);

	//Create directory for multiple images to avoid clutter
	if(image_num > 1){
		char dirname[30];
		sprintf(dirname,"%s_images",name);
		mkdir(dirname, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		chdir(dirname);
	}

	for(int i = 0; i < image_num; i++){
		sem_wait(sem1);
		pid_t pid = fork();		//fork into desired number of processes
		if (pid < 0){
			printf("FORK failed, terminating...\n");
			exit(1);
			
		} else if(pid == 0){ //CHILD
			child_process(sem1, outfile, name, i, xscale, yscale, xcenter, ycenter, image_width, image_height, max, num_threads);
		} else {
			//reconfigure for next image
			xscale-=(xscale*0.05);	// zoom in by 5%
		}
	}

	//loop to guarantee all images are generated before parent exits
	for (int j=0; j<image_num; j++){
        wait(0);
    }
	printf("Done!\n");
	//unlink and close the semaphore
	sem_close(sem1);
	sem_unlink("/lotsofkids");
	return 0;
}




/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iter;
}

/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image(imgRawImage* img, double xmin, double xmax, double ymin, double ymax, int max, int num_threads)
{
	pthread_t threadArray[20];
	image_parameters imgArray[20];
	int rc; 

	int width = img->width;
	int height = img->height;
	printf("IMAGE WIDTH: %d\nIMAGE HEIGHT: %d\n", width, height);
	printf("max: %d\n", max);

	//thread specific calculations
	int thread_height_max = height/num_threads;
	int thread_height_min = 0;

	for(int i = 0; i<num_threads;i++){
		image_parameters curImage = {img, height, width, xmin, xmax, ymin, ymax, max, num_threads, thread_height_min, thread_height_max};
		imgArray[i] = curImage;

		if ((rc=pthread_create(&threadArray[i], NULL, &threadmethod, &imgArray[i])))
		{
			printf("Thread %d creation failed\n",i);
		}

		//set parameters for next thread
		thread_height_min = thread_height_max;
		if(i == num_threads-2){
			thread_height_max = height;
		} else {
			thread_height_max += (height/num_threads);
		}
	}

	//wait for the threads to exit
	for(int i = 0; i<num_threads; i++){
		pthread_join(threadArray[i], NULL);
	}

}


/*
Convert a iteration number to a color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/
int iteration_to_color( int iters, int max )
{
	int color = 0xFF3FFF*iters/(double)max;
	return color;
}


// Show help message
void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates (X-axis). (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=1000)\n");
	printf("-H <pixels> Height of the image in pixels. (default=1000)\n");
	printf("-o <file>   Set output file. (default=mandel.jpg)\n");
	printf("-i <number>   Set the number of images to be generated. file. (default=1)\n");
	printf("	Each consecutive image zooms the mandel by 5 percent.\n");
	printf("-c <number>   Set number of consecutive children. (default=5)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}

// Child Process
void child_process(sem_t* sem1, char outfile[50], char name[15], int i,
	double xscale, double yscale, double xcenter, double ycenter,
			int image_width, int image_height, int max, int num_threads){
			// Create a name for the files
			sprintf(outfile, "%s%d.jpg", name, i);

			// Calculate y scale based on x scale (settable) and image sizes in X and Y (settable)
			yscale = xscale / image_width * image_height;

			// Display the configuration of the image.
			printf("mandel: x=%lf y=%lf xscale=%lf yscale=%1f max=%d outfile=%s\n",xcenter,ycenter,xscale,yscale,max,outfile);

			// Create a raw image of the appropriate size.
			imgRawImage* img = initRawImage(image_width,image_height);

			// Fill it with a white
			setImageCOLOR(img,0xFFFFFF);

			// Compute the Mandelbrot image
			compute_image(img,xcenter-xscale/2,xcenter+xscale/2,ycenter-yscale/2,ycenter+yscale/2,max,num_threads);

			// Save the image in the stated file.
			storeJpegImageFile(img,outfile);

			// free the mallocs
			freeRawImage(img);

	sem_post(sem1);
	int close_status = sem_close(sem1);
	if(close_status == -1){
		printf("sem1 failed to close in child %i\n",i);
	}
	return;
}

//Thread Process
void* threadmethod(void* vp){
	image_parameters curImage = *(image_parameters*)vp;
	//unpack parameters
	imgRawImage* img = curImage.img;
	int height = curImage.height;
	int width = curImage.width;
	double xmin = curImage.xmin;
	double xmax = curImage.xmax;
	double ymin = curImage.ymin;
	double ymax = curImage.ymax;
	int max = curImage.max;
	int thread_height_min = curImage.thread_height_min;
	int thread_height_max = curImage.thread_height_max;
	int i = 0;
	int j = 0;
	// For every pixel in the image the thread is responsible for...
	for(j=thread_height_min;j<thread_height_max;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = xmin + i*(xmax-xmin)/width;
			double y = ymin + j*(ymax-ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,max);

			// Set the pixel in the bitmap.
			setPixelCOLOR(img,i,j,iteration_to_color(iters,max));
		}
	}
	
	return NULL;
}
