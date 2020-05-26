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


#include <thread>         // std::thread

#include <Processing.NDI.Lib.h>

#include <sched.h>


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
bool newFrame = false;

FrameCanvas *offscreen_canvas;
RGBMatrix *matrix;

volatile bool interrupt_received = false;
static void InterruptHandler(int) {
  interrupt_received = true;

}



void set_realtime_priority() {
     int ret;
 
     // We'll operate on the currently running thread.
     pthread_t this_thread = pthread_self();



     // struct sched_param is used to store the scheduling priority
     struct sched_param params;
 
     // We'll set the priority to the maximum.
     params.sched_priority = sched_get_priority_max(SCHED_RR);
  std::cout << "Trying to set thread realtime prio = " << params.sched_priority << std::endl;
 
     // Attempt to set thread real-time priority to the SCHED_RR policy
     ret = pthread_setschedparam(this_thread, SCHED_RR, &params);
     if (ret != 0) {
         // Print the error
         std::cout << "Unsuccessful in setting thread realtime prio" << std::endl;
         return;     
     }

 // Now verify the change in thread priority
     int policy = 0;
     ret = pthread_getschedparam(this_thread, &policy, &params);
     if (ret != 0) {
         std::cout << "Couldn't retrieve real-time scheduling paramers" << std::endl;
         return;
     }
 
     // Check the correct policy was applied
     if(policy != SCHED_RR) {
         std::cout << "Scheduling is NOT SCHED_RR!" << std::endl;
     } else {
         std::cout << "SCHED_RR OK" << std::endl;
     }
 
     // Print thread scheduling priority
     std::cout << "Thread priority is " << params.sched_priority << std::endl; 
}



void set_low_priority() {
     int ret;
 
     // We'll operate on the currently running thread.
     pthread_t this_thread = pthread_self();



     // struct sched_param is used to store the scheduling priority
     struct sched_param params;
 
     // We'll set the priority to the maximum.
     params.sched_priority = sched_get_priority_max(SCHED_IDLE);
  std::cout << "Trying to set thread realtime prio = " << params.sched_priority << std::endl;
 
     // Attempt to set thread real-time priority to the SCHED_IDLE policy
     ret = pthread_setschedparam(this_thread, SCHED_IDLE, &params);
     if (ret != 0) {
         // Print the error
         std::cout << "Unsuccessful in setting thread realtime prio" << std::endl;
         return;     
     }

 // Now verify the change in thread priority
     int policy = 0;
     ret = pthread_getschedparam(this_thread, &policy, &params);
     if (ret != 0) {
         std::cout << "Couldn't retrieve real-time scheduling paramers" << std::endl;
         return;
     }
 
     // Check the correct policy was applied
     if(policy != SCHED_IDLE) {
         std::cout << "Scheduling is NOT SCHED_IDLE!" << std::endl;
     } else {
         std::cout << "SCHED_IDLE OK" << std::endl;
     }
 
     // Print thread scheduling priority
     std::cout << "Thread priority is " << params.sched_priority << std::endl; 
}



void CopyFrame(NDIlib_video_frame_v2_t * video_frame, FrameCanvas * canvas) {

copying = true;
//printf("copying frame to canvas\n");
for(int h=0; h < 128; h++)
{
  for(int w=0; w < 128; w++)
  {
    int indexB = (h*video_frame->xres + w)*4;
    int indexG = (h*video_frame->xres + w)*4+1;
    int indexR = (h*video_frame->xres + w)*4+2;

  
    canvas->SetPixel(w, h, video_frame->p_data[indexR], video_frame->p_data[indexG], video_frame->p_data[indexB]);

  }	
}
copying = false;

}

int runNDIReceiver(std::string sourceName, int width, int height)
{

	set_low_priority();
	
	
	
	if (!NDIlib_initialize()) return 0;
	
	// Create a finder
	NDIlib_find_instance_t pNDI_find = NDIlib_find_create_v2();
	if (!pNDI_find) return 0;

	// Wait until there is one source
	uint32_t no_sources = 0;
	const NDIlib_source_t* p_sources = NULL;
	const NDIlib_source_t* p_requested_source = NULL;
	
	while (p_requested_source == NULL)
	{	// Wait until the sources on the nwtork have changed
		printf("Looking for sources ...\n");
		NDIlib_find_wait_for_sources(pNDI_find, 1000/* One second */);
		p_sources = NDIlib_find_get_current_sources(pNDI_find, &no_sources);
	
		for (int i = 0; i < no_sources; i++)
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
	if (!pNDI_recv) return 0;

	// Connect to our sources
	NDIlib_recv_connect(pNDI_recv, p_requested_source);

	// Destroy the NDI finder. We needed to have access to the pointers to p_sources[0]
	NDIlib_find_destroy(pNDI_find);	



	unsigned char* data;

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
				
				//display here video_frame
				data = video_frame.p_data;
				//printf("Frame size : %d\n", video_frame.line_stride_in_bytes);
				//printf("Frame fourCC : %d\n", video_frame.FourCC);
				
				//printf("going thru loop width : %d\n", finalImage.width);
				for(int x = 0; x < video_frame.xres; x++)
				{
					for(int y = 0; y < video_frame.yres; y++)
					{
						//printf ("x : %d, y : %d", x, y);
						//int indexB = (y*finalImage.width + x)*2+1;
					    int indexB = (y*video_frame.xres + x)*4;
					    int indexG = (y*video_frame.xres + x)*4 + 1;
 					    int indexR = (y*video_frame.xres + x)*4 + 2;


							
					}
				}
				
				
				//if(syncing == false)
				//{
					CopyFrame(&video_frame, offscreen_canvas);
					newFrame = true;
				//}

				
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





int runMatrix()
{

  set_realtime_priority();
	
  offscreen_canvas = matrix->CreateFrameCanvas();


  int vsync_multiple = 1;
  

//Main loop

  printf("Entering main loop thread\n");  
  do {

//	if(copying == false)
//	{
		//syncing = true;//TODO : instead of syncing, swap with temporary buffer
		//if(newFrame == true)
		//{
        		offscreen_canvas = matrix->SwapOnVSync(offscreen_canvas,
                                                	   vsync_multiple);
		//	newFrame = false;
		//}
		//syncing = false;
//	}											   
  } while (!interrupt_received);

}




int main(int argc, char *argv[]) {

  signal(SIGTERM, InterruptHandler);
  signal(SIGINT, InterruptHandler);


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
	
	
	
	 
//Start MATRIX loop
  std::thread matrixThread (runMatrix);  
  matrixThread.detach();

  std::thread NDIThread (runNDIReceiver, "OBS-SLYZIC", 128, 128);  
  NDIThread.detach();


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
