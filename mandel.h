#ifndef MANDEL_H
#define MANDEL_H

//static void compute_image( imgRawImage *img, double xmin, double xmax,
//					double ymin, double ymax, int max, int num_threads);
int mandelmain(double x_center, double y_center, double scale, int max_iter, int img_num);

#endif /* Compile Guard */