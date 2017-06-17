# Image-Processing-with-MPI
Distributed image processing (blur filter) using the Message Passing Interface (MPI) in C. The program utilizes a cluster of computers to evently distributed an image, apply a blur filter based on radius r, and reassembles the image on the host.   
   
Usage: mpirun -np ./pa4.x r (input image).ppm (output image).ppm

## Program Function:
The program accepts an input image in ppm format.  
The image is split evenly among computers available on the MPI cluster.  
Each computer applies a blur filter on pixels assigned to it based on blur radius r.  
Upon completion the image is reassembled by the host computer and saved in an output ppm file.  
