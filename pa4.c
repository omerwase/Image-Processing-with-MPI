/*
 * Programming Assignment 4 for CS 6F03 Winter 2017 Term
 * Filename: pa4.c
 * By: Omer Waseem (#000470449) and Erica Cheyne  (#001201341)
 * Date Submitted:
 * Description:
 * 	- Image blurred based on given radius using n nodes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include "ppmFile.h"


int main (int argc, char *argv[]) {
	
	int comm_sz, my_rank, w, h, x, y, i, j, chan, r, temp, num, rowSize, row0Size, offset;
	Image *inImage;
	Image *outImage;
	Image *tempImage;
	unsigned char *data; // image data
	int *rowSizes; // vector containing image row split sizes for each process
	int params[3]; // parameter vector for processes (w, r, rowSize)
	unsigned char *section; // partial image section for each process
	unsigned char *blurred; // blurred image section from each process
	double local_start, local_finish, local_elapsed, elapsed;
	
	MPI_Init(NULL, NULL);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	
	// check for correct number of input arguments
	if (argc != 4) {
		if (my_rank == 0) {
			printf("Incorrect input arguments. Should be: r <in image>.ppm <out image>.ppm\n");
		}
		MPI_Finalize();
		return 0;
	}
	
	// Process 0 reads the image and determines how the image will be split between other processes.
	// Image rows are split as evenly as possible. In order to minimize communication between
	// processes, rows are assigned in chunks (using block distribution) instead of assigning row by
	// row to a process and cycling through the image. Process 0 alone handles the edges since it
	// alone has access to the entire image. Because process 0 has to handle this extra computaiton
	// the number of rows assigned to process 0 are reduced based on r to balance the load
	if (my_rank == 0) {
		
		r = atoi(argv[1]);
		
		// read image and obtain dimensions
		inImage = ImageRead(argv[2]);
		w = inImage->width;
		h = inImage->height;
		data = inImage->data;
		printf("Using image: %s, width: %d, height: %d, blur radius: %d\n",argv[2],w,h,r);
		
		// int array to create partition size for each process
		rowSizes = (int*)malloc(sizeof(int*)*comm_sz);
		num = (h - 2*r) / comm_sz; // number of rows to assign each process
		temp = (h - 2*r) % comm_sz; // remaining rows incase of uneven division
		for (x = comm_sz-1; x >= 0; x--) {
			rowSizes[x] = num;
			
			// incase of uneven division of rows, assign extra rows to processes starting with the
			// highest rank. As such process 0 will never be assigned an extra row. This compensates
			// for the extra computation required to process image edges.
			if (temp > 0) {
				rowSizes[x]++;
				temp--;
			}
		}
		
		// balance the load on process 0 further based on r
		x = 1;
		temp = (r-1)*2;
		while (rowSizes[0] > 0 && temp > 0 && comm_sz > 1) {
			rowSizes[0]--;
			rowSizes[x]++;
			temp--;
			x++;
			if (x == comm_sz) {
				x = 1;
			}
		}
		
		rowSize = rowSizes[0];
		row0Size = rowSize + 2*r; // for printing purposes
		printf("Process 0 of %d assigned parameters: w: %d, r: %d, row size: %d\n", comm_sz, w, r, row0Size);
		
		// send relevent parameters to processes > 0
		params[0] = w;
		params[1] = r;
		for (x = 1; x < comm_sz; x++) {
			params[2] = rowSizes[x];
			MPI_Send(params, 3, MPI_INT, x, 0, MPI_COMM_WORLD);
		}
		
		// send sections of image to processes > 0
		offset = rowSize;
		for (x = 1; x < comm_sz; x++) {
			temp = w * (rowSizes[x]+2*r) * 3;
			section = (unsigned char*)malloc(sizeof(unsigned char*)*temp);
			memcpy(section, data + w*(offset)*3, temp);
			MPI_Send(section, temp, MPI_UNSIGNED_CHAR, x, 0, MPI_COMM_WORLD);
			free(section);
			offset += rowSizes[x];
			
			printf("Process %d of %d assigned parameters: w: %d, r: %d, row size: %d\n", x, comm_sz, w, r, rowSizes[x]);
		}
		
		printf("Bluring image using %d node(s) ...\n",comm_sz);
		
		// copy section for process 0
		temp = w * (rowSize+2*r) * 3;
		section = (unsigned char*)malloc(sizeof(unsigned char*)*temp);
		memcpy(section, data, temp);
		
	} else { // for processes > 0
		// recieve parameters from process 0
		MPI_Recv(params, 3, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		w = params[0];
		r = params[1];
		rowSize = params[2];
		
		// recieve image section from process 0
		temp = w * (rowSize+2*r) * 3;
		section = (unsigned char*)malloc(sizeof(unsigned char*)*temp);
		MPI_Recv(section, w * (rowSize+2*r) * 3, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	
	// the code below (lines 141 to 164) is the blurring executed in parallel by all processes
	
	// timing code
	MPI_Barrier(MPI_COMM_WORLD);
	local_start = MPI_Wtime();
	
	// create temporary image for each process
	tempImage = ImageCreate(w, rowSize);
	ImageClear(tempImage, 255, 255, 255);
	
	// blur image section for each process into tempImage
	for (j = 0; j < rowSize; j++) {
		for (i = 0; i < w; i++) {
			for (chan = 0; chan < 3; chan++) {
				temp = 0;
				num = 0;
				for (y = j; y <= j+2*r; y++) {
					for (x = i-r; x <= i+r; x++) {
						if (x >= 0 && x < w) {
							offset = (y*w+x)*3+chan;
							temp += section[offset];
							num++;
						}
					}
				}
				temp /= num;
				ImageSetPixel(tempImage,i,j,chan,temp);
			}
		}
	}
	free(section);
	
	local_finish = MPI_Wtime();
	local_elapsed = local_finish - local_start;
	MPI_Reduce(&local_elapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	if (my_rank == 0) {
		printf("Elapsed time: %e seconds\n", elapsed);
	}
	
	// send tempImage->data back to process 0
	if (my_rank != 0) { // for processes > 0
		
		blurred = tempImage->data;
		temp = w*rowSize*3;
		MPI_Send(blurred, temp, MPI_UNSIGNED_CHAR, 0, 0, MPI_COMM_WORLD);
		
	} else { // for process 0
		
		// create new image for output
		outImage = ImageCreate(w, h);
		ImageClear(outImage, 255, 255, 255);
		
		// blur top edge
		for (j = 0; j < r; j++) {
			for (i = 0; i < w; i++) {
				for (chan = 0; chan < 3; chan++) {
					temp = 0;
					num = 0;
					for (y = j-r; y <= j+r; y++) {
						for (x = i-r; x <= i+r; x++) {
							if (x >= 0 && y >= 0 && x < w && y < h) {
								temp += ImageGetPixel(inImage,x,y,chan);
								num++;
							}
						}
					}
					temp /= num;
					ImageSetPixel(outImage,i,j,chan,temp);
				}
			}
		}
		
		// blur bottom edge
		for (j = h-1; j > h-1-r; j--) {
			for (i = 0; i < w; i++) {
				for (chan = 0; chan < 3; chan++) {
					temp = 0;
					num = 0;
					for (y = j-r; y <= j+r; y++) {
						for (x = i-r; x <= i+r; x++) {
							if (x >= 0 && y >= 0 && x < w && y < h) {
								temp += ImageGetPixel(inImage,x,y,chan);
								num++;
							}
						}
					}
					temp /= num;
					ImageSetPixel(outImage,i,j,chan,temp);
				}
			}
		}
		
		// concatinate blurred image from process 0 with outImage
		printf("Blurring complete, assembling image\n");
		offset = r;
		temp = w*rowSize*3;
		memcpy(outImage->data + w*(offset)*3, tempImage->data, temp);
		printf("Process 0 of %d added %d row(s) to blurred image\n", comm_sz, row0Size);
		
		// recieve blurred image sections and concatinate with outImage
		offset = rowSize + r;
		for (x = 1; x < comm_sz; x++) {
			temp = w*rowSizes[x]*3;
			blurred = (unsigned char*)malloc(sizeof(unsigned char*)*temp);
			MPI_Recv(blurred, temp, MPI_UNSIGNED_CHAR, x, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			memcpy(outImage->data + w*(offset)*3, blurred, temp);
			printf("Process %d of %d added %d row(s) to blurred image\n", x, comm_sz, rowSizes[x]);
			free(blurred);
			offset += rowSizes[x];
		}
		
		// write blurred image and free allocated image memory
		ImageWrite(outImage, argv[3]);
		printf("Blurred image created: %s\n", argv[3]);
		free(inImage->data);
		free(outImage->data);
	}

	free(tempImage->data);
	
	MPI_Finalize();
	
	return 0;
}
