# Image-Processing-with-MPI
Distributed image processing (blur filter) using the Message Passing Interface (MPI) in C. The program utilizes a cluster of computers to evently distributed an image, apply a blur filter based on radius r, and reassembles the image on the host.   
   
Usage: mpirun -np ./pa4.x r (input image).ppm (output image).ppm

##
