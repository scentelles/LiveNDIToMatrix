// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
//

#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


#include <string>
#include <vector>
#include <cstring>
#include <stdio.h>
#include <iostream>


#include "opencv2/opencv.hpp"
using namespace cv;


#include <thread>         // std::thread

#include <Processing.NDI.Lib.h>

#include <sched.h>

#include <libconfig.h++>
using namespace libconfig;

using std::endl;
using std::cout;
using std::cerr;


#include "led-matrix.h"
#include "content-streamer.h"

using rgb_matrix::FrameCanvas;
using rgb_matrix::RGBMatrix;
using rgb_matrix::StreamWriter;
using rgb_matrix::StreamIO;


bool mplayerStarted = false;
bool projectMStarted = false;
bool imageStarted = false;

#define UPDATE_MULTIPLE 2
bool copying = false;
bool syncing = false;
//bool newFrame = false;

float alpha 	= 1;
float red 	= 1;
float green 	= 1;
float blue 	= 1;
int   strobe	= 0;

FrameCanvas *offscreen_canvas1;
FrameCanvas *offscreen_canvas2;
FrameCanvas **active_frame_ref;
FrameCanvas *canvas_black;

RGBMatrix *matrix;

#define OSCPKT_OSTREAM_OUTPUT
#include "oscpkt/oscpkt.hh"
#include "oscpkt/udp.hh"
using namespace oscpkt;
using namespace std;

UdpSocket sock; 
int OSC_PORT_NUM = 0;


string OSC_CONTROL_ALPHA;
string OSC_CONTROL_RED;
string OSC_CONTROL_GREEN;
string OSC_CONTROL_BLUE;
string OSC_CONTROL_STROBE;


string OBS_NDI_SOURCE;

volatile bool interrupt_received = false;
static void InterruptHandler(int) {
  interrupt_received = true;

}


void set_priority(int priority) {
     int ret;
 
     // We'll operate on the currently running thread.
     pthread_t this_thread = pthread_self();

     // struct sched_param is used to store the scheduling priority
     struct sched_param params;
 
     // We'll set the priority to the maximum.
     params.sched_priority = sched_get_priority_max(priority);
     std::cout << "Trying to set thread prio = " << params.sched_priority << std::endl;
 
     // Attempt to set thread priority to the SCHED_RR policy
     ret = pthread_setschedparam(this_thread, priority, &params);
     if (ret != 0) {
         // Print the error
         std::cout << "Unsuccessful in setting thread prio" << std::endl;
         return;     
     }

     // Now verify the change in thread priority
     int policy = 0;
     ret = pthread_getschedparam(this_thread, &policy, &params);
     if (ret != 0) {
         std::cout << "Couldn't retrieve scheduling parameters" << std::endl;
         return;
     }
 
     // Check the correct policy was applied
     if(policy != priority) {
         std::cout << "Scheduling is NOT !" << priority << std::endl;
     } else {
         std::cout << "Scheduling prio change OK" << priority << std::endl;
     }
 
     // Print thread scheduling priority
     std::cout << "Thread priority is " << params.sched_priority << std::endl; 
}


//TODO : use a json config file
void runOSCServer() {

    sock.bindTo(OSC_PORT_NUM);
    if (!sock.isOk()) {
      cerr << "Error opening port " << OSC_PORT_NUM << ": " << sock.errorMessage() << "\n";
    }
    else {
      cout << "Server started, will listen to packets on port " << OSC_PORT_NUM << std::endl;
      PacketReader pr;
      PacketWriter pw;
      while (sock.isOk()) {      
	if (sock.receiveNextPacket(30 /* timeout, in ms */)) {
          pr.init(sock.packetData(), sock.packetSize());
          oscpkt::Message *msg;
          while (pr.isOk() && (msg = pr.popMessage()) != 0) {
              //float tempF;
	      int tempI;
              cout << "ADDRESS : " << msg->address << endl;

	      if (msg->match(OSC_CONTROL_ALPHA).popInt32(tempI)) {

		    alpha = float(tempI)/255;
		    cout << "Setting alpha : " << alpha << endl;
	      }
	      if (msg->match(OSC_CONTROL_RED).popInt32(tempI)) {

		    red = float(tempI)/255;
	    	    cout << "Setting Red Multiply : " << red << endl;
	      }	  
	      if (msg->match(OSC_CONTROL_GREEN).popInt32(tempI)) {

		    green = float(tempI)/255;
	    	    cout << "Setting Green Multiply  : " << green << endl;
	      }		  
	      if (msg->match(OSC_CONTROL_BLUE).popInt32(tempI)) {

		    blue = float(tempI)/255;
	    	    cout << "Setting Blue Multiply  : " << blue << endl;
	      }	
	      if (msg->match(OSC_CONTROL_STROBE).popInt32(tempI)) {

		    strobe = tempI;
	    	    cout << "Setting Strobe  : " << strobe << endl;
	      }	

          }
        }
      }
    }
  
}





void CopyFrame(NDIlib_video_frame_v2_t * video_frame, FrameCanvas * canvas) {

    copying = true;
    for(int h=0; h < canvas->height(); h++)
    {
      for(int w=0; w < canvas->width(); w++)
      {
	int indexB = (h*video_frame->xres + w)*4;
	int indexG = (h*video_frame->xres + w)*4+1;
	int indexR = (h*video_frame->xres + w)*4+2;

	//canvas->SetPixel(w, h, (std::min(video_frame->p_data[indexR]+int(red), 255))*alpha, (std::min(video_frame->p_data[indexG]+ int(green), 255))*alpha, (std::min(video_frame->p_data[indexB]+int(blue), 255))*alpha);

	canvas->SetPixel(w, h, video_frame->p_data[indexR]*red*alpha, video_frame->p_data[indexG]*green*alpha, video_frame->p_data[indexB]*blue*alpha);

      }	
    }
    copying = false;

}

void CopyFrame(Mat * video_frame, FrameCanvas * canvas) {

    copying = true;
    for(int h=0; h < canvas->height(); h++)
    {
      for(int w=0; w < canvas->width(); w++)
      {
	//int indexB = (h*video_frame->xres + w)*4;
	//int indexG = (h*video_frame->xres + w)*4+1;
	//int indexR = (h*video_frame->xres + w)*4+2;
unsigned char * p = video_frame->ptr(h, w); // Y first, X after
//p[0] = 0;   // B
//p[1] = 0;   // G
//p[2] = 255; // R

	//canvas->SetPixel(w, h, (std::min(video_frame->p_data[indexR]+int(red), 255))*alpha, (std::min(video_frame->p_data[indexG]+ int(green), 255))*alpha, (std::min(video_frame->p_data[indexB]+int(blue), 255))*alpha);

	//canvas->SetPixel(w, h, video_frame->p_data[indexR]*red*alpha, video_frame->p_data[indexG]*green*alpha, video_frame->p_data[indexB]*blue*alpha);
	canvas->SetPixel(w, h, p[2]*red*alpha, p[1]*green*alpha,p[0]*blue*alpha);

      }	
    }
    copying = false;

}


void runUVCReceiver()
{
    printf("Opening\n");
    
    VideoCapture cap(0); // open the default camera
    
    if (!cap.isOpened())  // check if we succeeded
    {
        printf("ERROR : could not open capture device\n");
	return;
    }
    cap.set(CV_CAP_PROP_BUFFERSIZE, 3);
    
    printf("Opened\n");
    //Ptr<BackgroundSubtractor> pMOG = new BackgroundSubtractorMOG2();

    Mat fg_mask;
    Mat frame;
    int count = -1;

    for (;;)
    {
        // Get frame
        cap >> frame; // get a new frame from camera

        // Update counter
        ++count;
	//if (count%2 == 0)
	//  continue;

        // Background subtraction
        //pMOG->operator()(frame, fg_mask);

       // imshow("frame", frame);
	
   // printf("new frame %d \n", count);

        //imshow("fg_mask", fg_mask);

        // Save foreground mask
      //  string name = "mask_" + std::to_string(count) + ".png";
      //  imwrite("D:\\SO\\temp\\" + name, fg_mask);
//waitKey(1);
       // if (waitKey(1) >= 0) break;
       
       
       				if(active_frame_ref == &offscreen_canvas1)
				{
				  //cout << "copying to frame 2 " << endl;
				  CopyFrame(&frame, offscreen_canvas2);
				  active_frame_ref = &offscreen_canvas2;
				}
				else
				{
				  //cout << "copying to frame 1 " << endl;
				  CopyFrame(&frame, offscreen_canvas1);
				  active_frame_ref = &offscreen_canvas1;
				}

    }
}



void runNDIReceiver(std::string sourceName)
{

	set_priority(SCHED_IDLE);
	
	
	if (!NDIlib_initialize())
	{
	    cout << "ERROR : Failed to intialize NDI" << endl;
	    return;
	}
	// Create a finder
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2();
	if (!pNDI_find)
	{
	    cout << "ERROR : Failed to create NDI finder" << endl;
	    return;
	}
	// Wait until there is one source
	uint32_t no_sources = 0;
	const NDIlib_source_t* p_sources = NULL;
	const NDIlib_source_t* p_requested_source = NULL;
	
	while (p_requested_source == NULL)
	{	// Wait until the sources on the nwtork have changed
		printf("Looking for sources ...\n");
		NDIlib_find_wait_for_sources(pNDI_find, 1000/* One second */);
		p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
	
		for (unsigned int i = 0; i < no_sources; i++)
		{
			std::string tempS = p_sources[i].p_ndi_name;
			printf("%u. %s\n", i+1, tempS.c_str());
			if(tempS.find(sourceName) != std::string::npos)
			{
				printf("FOUND REQUESTED NDI SOURCE : %s in %s", sourceName.c_str(), tempS.c_str()); 
				p_requested_source = &p_sources[i];
			}
		}
	}
	
	printf("source found ...\n");
	
	NDIlib_recv_create_v3_t setting;
	setting.color_format = NDIlib_recv_color_format_BGRX_BGRA;
	//setting.color_format = NDIlib_recv_color_format_fastest;
	setting.bandwidth = NDIlib_recv_bandwidth_lowest;
	//setting.bandwidth = NDIlib_recv_bandwidth_highest;
	
	NDIlib_recv_instance_t pNDI_recv = NDIlib_recv_create_v3(&setting);
	if (!pNDI_recv)
	{
	    cout << "ERROR : Failed to create NDI receiver instance" << endl;
	    return;
	}

	// Connect to our sources
	NDIlib_recv_connect(pNDI_recv, p_requested_source);

	// Destroy the NDI finder. We needed to have access to the pointers to p_sources[0]
	NDIlib_find_destroy(pNDI_find);	


	printf("starting to loop ...\n");
	while (!interrupt_received) //TODO : catch interrupt
	{	// The descriptors
		NDIlib_video_frame_v2_t video_frame;
		NDIlib_audio_frame_v2_t audio_frame;

		switch (NDIlib_recv_capture_v2(pNDI_recv, &video_frame, NULL, nullptr, 5000))
		{	// No data
			case NDIlib_frame_type_none:
				printf("No data received.\n");
				break;

			// Video data
			case NDIlib_frame_type_video:
			//	printf("Video data received (%dx%d).\n", video_frame.xres, video_frame.yres);
				if(active_frame_ref == &offscreen_canvas1)
				{
				  //cout << "copying to frame 2 " << endl;
				  CopyFrame(&video_frame, offscreen_canvas2);
				  active_frame_ref = &offscreen_canvas2;
				}
				else
				{
				  //cout << "copying to frame 1 " << endl;
				  CopyFrame(&video_frame, offscreen_canvas1);
				  active_frame_ref = &offscreen_canvas1;
				}

				
				NDIlib_recv_free_video_v2(pNDI_recv, &video_frame);
				break;

			// Audio data
			case NDIlib_frame_type_audio:
				//printf("Audio data received (%d samples).\n", audio_frame.no_samples);
				//NDIlib_recv_free_audio_v2(pNDI_recv, &audio_frame);
				break;
			default:
				printf("unsupported frame type\n");
		}
	}
	
	if (interrupt_received) {
    	// Feedback for Ctrl-C, but most importantly, force a newline
    	// at the output, so that commandline-shell editing is not messed up.
    	fprintf(stderr, "Got interrupt. Exiting\n");
	

	// Destroy the receiver
	NDIlib_recv_destroy(pNDI_recv); 

	// Not required, but nice
	NDIlib_destroy(); 
	
  }
	
}







struct LedPixel {
  uint8_t r, g, b;
};





//TODO : update usage
static int usage(const char *progname, const char *msg = nullptr) {
  if (msg) {
    fprintf(stderr, "%s\n", msg);
  }
  fprintf(stderr, "usage: %s [options] <video>\n", progname);
  fprintf(stderr, "Options:\n"
          "\t-F                 : Full screen without black bars; aspect ratio might suffer\n"
          "\t-O<streamfile>     : Output to stream-file instead of matrix (don't need to be root).\n"
          "\t-s <count>         : Skip these number of frames in the beginning.\n"
          "\t-c <count>         : Only show this number of frames (excluding skipped frames).\n"
          "\t-V<vsync-multiple> : Instead of native video framerate, playback framerate\n"
          "\t                     is a fraction of matrix refresh. In particular with a stable refresh,\n"
          "\t                     this can result in more smooth playback. Choose multiple for desired framerate.\n"
          "\t                     (Tip: use --led-limit-refresh for stable rate)\n"
          "\t-f                 : Loop forever.\n");

  fprintf(stderr, "\nGeneral LED matrix options:\n");
  rgb_matrix::PrintMatrixFlags(stderr);
  return 1;
}


void fillCanvas(FrameCanvas * canvas, int R, int G, int B) {
    for(int h=0; h < canvas->height(); h++)
    {
      for(int w=0; w < canvas->width(); w++)
      {
	canvas->SetPixel(w, h, R, G, B);

      }	
    }

}
void fillRandomSquare(FrameCanvas * canvas, int squareSize, int R, int G, int B) {

    int yPosition = rand() % canvas->height() - squareSize;
    int xPosition = rand() % canvas->width()  - squareSize;

    for(int h=0; h < canvas->height(); h++)
    {
      for(int w=0; w < canvas->width(); w++)
      {
	canvas->SetPixel(w, h, 0, 0, 0);

      }	
    }
    for(int h = yPosition; h < yPosition + squareSize; h++)
    {
      for(int w = xPosition; w < xPosition + squareSize; w++)
      {
	canvas->SetPixel(w, h, R, G, B);

      }	
    }

}


void runMatrix()
{

  set_priority(SCHED_FIFO);
	
  offscreen_canvas1 = matrix->CreateFrameCanvas();
  offscreen_canvas2 = matrix->CreateFrameCanvas();  
  canvas_black = matrix->CreateFrameCanvas();  
  
  fillCanvas(canvas_black, 0, 0, 0);
 // fillRandomSquare(canvas_black, 64, 255, 255, 255);
  
  active_frame_ref = &offscreen_canvas1;
  printf("Started matrix with resolution : w:%d, h:%d\n", offscreen_canvas1->width(), offscreen_canvas1->height()); 


  int vsync_multiple = 1;
  

//Main loop
  int count = 0;
  printf("Entering main loop thread\n");  
  do {
	count++;
//	if(copying == false)
//	{
		//syncing = true;//TODO : instead of syncing, swap with temporary buffer
		//if(newFrame == true)
		//{
        		//*active_frame_ref = matrix->SwapOnVSync(*active_frame_ref,
                        //                        	   vsync_multiple);
			
			if(strobe != 0)
			{
				if(count < strobe)
				{
			    		matrix->SwapOnVSync(*active_frame_ref, vsync_multiple);
				}
				else
				{
			    		matrix->SwapOnVSync(canvas_black, vsync_multiple);

				}
				if(count > strobe *2)
				{
			    		count = 0;
					//fillRandomSquare(canvas_black, 64, 255, 255, 255);

				}
			}
			else
			{
			     matrix->SwapOnVSync(*active_frame_ref, vsync_multiple);
			
			}

			//newFrame = true;
		//}
		//syncing = false;
//	}											   
  } while (!interrupt_received);

}

int readConfigInt(Config &cfg, string name)
{
  int result=0;
  try
  {
    result = cfg.lookup(name);
    cout << "Config : " << name << " : " << result << endl;
  }
  catch(const SettingNotFoundException &nfex)
  {
    cerr << "No " << name << " setting in configuration file." << endl;
  }
  return result;

}


string readConfigString(Config &cfg, string name)
{
  string result;
  try
  {
    string tempS = cfg.lookup(name);
    result = tempS;
    cout << "Config : " << name << " : " << result << endl;
  }
  catch(const SettingNotFoundException &nfex)
  {
    cerr << "No " << name << " setting in configuration file." << endl;
  }
  return result;

}

int main(int argc, char *argv[]) {

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);

  Config cfg;

  // Read the file. If there is an error, report it and exit.
  try
  {
    cfg.readFile("config.cfg");
  }
  catch(const FileIOException &fioex)
  {
    std::cerr << "I/O error while reading file." << std::endl;
    return(EXIT_FAILURE);
  }
  catch(const ParseException &pex)
  {
    std::cerr << "Parse error at " << pex.getFile() << ":" << pex.getLine()
              << " - " << pex.getError() << std::endl;
    return(EXIT_FAILURE);
  }

 
  OSC_PORT_NUM        = readConfigInt(cfg, "OSC_PORT_NUM");
  OSC_CONTROL_ALPHA   = readConfigString(cfg, "OSC_CONTROL_ALPHA");
  OSC_CONTROL_RED     = readConfigString(cfg, "OSC_CONTROL_RED");
  OSC_CONTROL_GREEN   = readConfigString(cfg, "OSC_CONTROL_GREEN");
  OSC_CONTROL_BLUE    = readConfigString(cfg, "OSC_CONTROL_BLUE");
  OSC_CONTROL_STROBE  = readConfigString(cfg, "OSC_CONTROL_STROBE");

  OBS_NDI_SOURCE    = readConfigString(cfg, "OBS_NDI_SOURCE");


  //RGB Matrix
  RGBMatrix::Options matrix_options;
  rgb_matrix::RuntimeOptions runtime_opt;
  if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv,
                                         &matrix_options, &runtime_opt)) {
    return usage(argv[0]);
  }


  int opt;
  while ((opt = getopt(argc, argv, "vO:R:Lfc:s:FV:")) != -1) {
    switch (opt) {
    default:
      return usage(argv[0]);
    }
  }

  matrix = CreateMatrixFromOptions(matrix_options, runtime_opt);
  if (matrix == NULL) {
    return 1;
  }
	
	
  //Start OSC server
  std::thread ocsThread (runOSCServer);     
  ocsThread.detach();
	 
  //Start MATRIX loop
  std::thread matrixThread (runMatrix);  
  matrixThread.detach();

  //Start NDI receive thread
 // std::thread NDIThread (runNDIReceiver, OBS_NDI_SOURCE);  
 // NDIThread.detach();
 
  //Start UVC receive thread
  std::thread UVCThread (runUVCReceiver);  
  UVCThread.detach();

  while(!interrupt_received)
  {
	  sleep(1);
  }
  if (interrupt_received) {
    // Feedback for Ctrl-C, but most importantly, force a newline
    // at the output, so that commandline-shell editing is not messed up.
    fprintf(stderr, "Got interrupt. Exiting\n");
	

	
  }

  delete matrix;
  

  return 0;
}
