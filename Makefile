OS	=	$(shell uname -s)
KARCH	=	$(shell uname -m)
NNAME 	=	$(shell uname -n)

# define some combined defines 
DOUBLE = -DDM_ARRAY_DOUBLE -DDIST_FFT_USE_DOUBLE

ifeq ($(OS),Darwin)
	ifeq ($(NNAME),portal2net.cluster.private) # for our cluster at BNL    
		#define ADDITIONAL includes and libraries
		INCLUDE_DIRS = -I/Volumes/RAID/sw/include
		LIB_DIRS = -L/Volumes/RAID/sw/lib

		#define compiler options
		CC = /usr/bin/mpicc
		CFLAGS = -faltivec -03 -mcpu=G5 -mtune=G5 \
			-mpowerpc64 -fstrict-aliasing -g
		LDFLAGS = -bind_at_load

		#define fft libs and dirs
		FFT_LIB = 
		FFT_LIB_DIRS = -L../../dist_fft/
		FFT_INCLUDE_DIRS = -I../../dist_fft/
		FFT_DEFINES = -D__APPLE__ -DDIST_FFT $(DOUBLE) \
                                -DDIST_FFT_USE_INTERLEAVED_COMPLEX
                FFT_FRAMEWORK = -framework Accelerate
		FFT_OBJS = dist_fft.o TOMS_transpose.o \
			dist_fft_transpose.o dist_fft_prefetch.o \
			dist_fft_twiddle.o sched.o transpose_mpi.o

		#define HDF5 libs and dirs
		HDF5_INCLUDE_DIR = -I/sw/hdf5/include/ 
		HDF5_LIB_DIR = -L/sw/hdf5/lib/
		HDF5_DEFINES = 
		HDF5_LIB = -lhdf5

		#if applicable, define MPI dirs and libs
		MPI_DEFINES = -D__MPI__
		MPI_LIB = -lmpi
		MPI_LIB_DIR = -L/usr/local/mpi/lib
		MPI_INCLUDE_DIR = -I/usr/local/mpi/include
	else # for any Mac single processor machine
		#define ADDITIONAL includes and libraries
		INCLUDE_DIRS = 
		LIB_DIRS = 

		#define compiler options
		CC = /usr/bin/cc 
		CFLAGS = -g
		LDFLAGS =

		#define fft libs and dirs
		FFT_LIB = -lfftw3f 
		FFT_LIB_DIRS = -L/usr/local/lib
		FFT_INCLUDE_DIRS = -I/usr/local/include
		FFT_DEFINES = 
		FFT_FRAMEWORK = 
		FFT_OBJS = 

		#define HDF5 libs and dirs
		HDF5_INCLUDE_DIR = -I/home/jan/SOFTWARE/hdf5/include/
		HDF5_LIB_DIR = -L/home/jan/SOFTWARE/hdf5/lib/
		HDF5_DEFINES = 
		HDF5_LIB = -lhdf5

		#if applicable, define MPI dirs and libs
		MPI_DEFINES = 
		MPI_LIB = 
		MPI_LIB_DIR = 
		MPI_INCLUDE_DIR = 
	endif
else # for machines of different OSs
	#define ADDITIONAL includes and libraries
	INCLUDE_DIRS = -I/usr/include -I/usr/local/include
	LIB_DIRS = -L/usr/local/lib -L/usr/lib

	#define compiler options
	CC = /usr/bin/gcc
	CFLAGS = -g
	LDFLAGS = 

	#define fft libs and dirs
	FFT_LIB = -lfftw3
	FFT_LIB_DIRS = -L/usr/local/lib
	FFT_INCLUDE_DIRS = -I/usr/local/include
	FFT_DEFINES = $(DOUBLE) -DVERBOSE
	FFT_FRAMEWORK = 
	FFT_OBJS = 

	#define HDF5 libs and dirs
	HDF5_INCLUDE_DIR = -I/home/jsteinb/hdf5/include/
	HDF5_LIB_DIR = -L/home/jsteinb/hdf5/lib/
	HDF5_DEFINES = 
	HDF5_LIB = -lhdf5

	#if applicable, define MPI dirs and libs
	MPI_DEFINES = 
	MPI_LIB =
	MPI_LIB_DIR = 
	MPI_INCLUDE_DIR = 
endif

# these are common for all
INCL_DIRS_ALL = -I.. -I../.. 
LIBS_ALL = -lpng -lm -lz
FFT_DIR=../../dist_fft/


cewald: cewald.o dm_fileio.o dm.o dm_array.o
	$(CC) -o cewald cewald.o dm_fileio.o dm.o dm_array.o \
	$(HDF5_LIB_DIR) $(LIB_DIRS) $(LIBS_ALL) \
	$(LIB_DIRS) $(HDF5_LIB) $(MPI_LIB) $(MPI_LIB_DIR) \
	$(FFT_LIB) $(FFT_LIB_DIRS)

cewald.o: cewald.c cewald.h
	$(CC) -c $(CFLAGS) cewald.c $(HDF5_INCLUDE_DIR) \
	$(FFT_INCLUDE_DIRS) $(FFT_DEFINES) $(HDF5_DEFINES) \
	$(INCLUDE_DIRS) $(MPI_DEFINES) $(MPI_INCLUDE_DIR)

dm_fileio.o: ../util/dm_fileio.c ../util/dm_fileio.h
	$(CC) -c $(CFLAGS) ../util/dm_fileio.c $(HDF5_INCLUDE_DIR) \
	$(FFT_INCLUDE_DIRS) $(FFT_DEFINES) $(HDF5_DEFINES) \
	$(INCLUDE_DIRS) $(HDF5_LIB) $(MPI_DEFINES) $(MPI_INCLUDE_DIR)

dm.o: ../util/dm.c ../util/dm.h
	$(CC) -c $(CFLAGS) ../util/dm.c $(HDF5_INCLUDE_DIR) \
	$(FFT_INCLUDE_DIRS) $(FFT_DEFINES) $(HDF5_DEFINES) \
	$(INCLUDE_DIRS) $(HDF5_LIB) $(MPI_DEFINES) $(MPI_INCLUDE_DIR) \
	$(LIBS_ALL)

dm_array.o: ../util/dm_array.c ../util/dm_array.h 
	$(CC) -c $(CFLAGS) ../util/dm_array.c $(FFT_INCLUDE_DIRS) \
	$(INCLUDE_DIRS)	$(FFT_DEFINES) $(TEST_DEFINES) \
	$(MPI_DEFINES) $(MPI_INCLUDE_DIR) $(INCL_DIRS_ALL)

clean:
	rm *.o
