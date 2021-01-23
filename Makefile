NDI_SDK_DIRECTORY=/home/pi/NDISDK/NDISDK2
NDI_SDK_INCLUDE=$(NDI_SDK_DIRECTORY)/include

CXXFLAGS=-Wall -O3 -Wextra -Wno-unused-parameter -D_FILE_OFFSET_BITS=64 `pkg-config opencv --cflags --libs`
#for profiling
#CXXFLAGS=-Wall -pg -O3 -g -Wextra -Wno-unused-parameter -D_FILE_OFFSET_BITS=64

OBJECTS=led-image-viewer.o text-scroller.o projectm-image-viewer.o app-launcher.o
BINARIES=UVCMatrix

OPTIONAL_OBJECTS=video-viewer.o projectm-video-viewer.o
OPTIONAL_BINARIES=video-viewer projectm-video-viewer

# Where our library resides. You mostly only need to change the
# RGB_LIB_DISTRIBUTION, this is where the library is checked out.
RGB_LIB_DISTRIBUTION=/home/pi/Project/rpi-rgb-led-matrix
RGB_INCDIR=$(RGB_LIB_DISTRIBUTION)/include 
RGB_LIBDIR=$(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME=rgbmatrix
RGB_LIBRARY=$(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a

LDFLAGS+=-L$(RGB_LIBDIR) -L$(NDI_SDK_DIRECTORY)/lib/arm-newtek-linux-gnueabihf  -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread -lndi -lconfig++

# Imagemagic flags, only needed if actually compiled.
MAGICK_CXXFLAGS?=`GraphicsMagick++-config --cppflags --cxxflags`
MAGICK_LDFLAGS?=`GraphicsMagick++-config --ldflags --libs`
AV_CXXFLAGS=`pkg-config --cflags  libavcodec libavformat libswscale libavutil`

all : $(BINARIES)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

UVCMatrix: UVCMatrix.o $(RGB_LIBRARY)
	$(CXX) $(CXXFLAGS) UVCMatrix.o -o $@ $(LDFLAGS) 


%.o : %.cc
	$(CXX) -I$(RGB_INCDIR) $(CXXFLAGS) -c -o $@ $<


UVCMatrix.o: UVCMatrix.cpp
	$(CXX) -I$(RGB_INCDIR) -I$(NDI_SDK_INCLUDE) $(CXXFLAGS) -Wno-deprecated-declarations -c -o $@ $<


clean:
	rm -f $(OBJECTS) $(BINARIES) $(OPTIONAL_OBJECTS) $(OPTIONAL_BINARIES)

FORCE:
.PHONY: FORCE
