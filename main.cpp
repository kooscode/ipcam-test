#include <stdio.h>
#include <iostream>
#include <opencv2/imgproc/imgproc_c.h>

#include "libterraclear/src/camera_file.hpp"
#include "libterraclear/src/camera_async.hpp"
#include "libterraclear/src/camera_recorder.hpp"

namespace tc = terraclear;

int main(int argc, char** argv) 
{
    //init OpenCV window
    char window_name[] = "cctv";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);// | cv::WINDOW_AUTOSIZE);
    
    //security cam
    tc::camera_file sec_cam("rtsp://USER_NAME:PASSWORD@IP_ADDRESS:80/cam/realmonitor?channel=1&subtype=0"); 
    
    //async camera thread
    tc::camera_async async_cam(&sec_cam);
    async_cam.thread_start("async-cam");
    
    //init start camcorder
    tc::camera_recorder camcorder;
    
    bool is_recording = false;
    int wait_detalay = 33;
    int frame_flash = 0;

    //display loop..
    for (;;)
    {
        //Get frame from Camera
        cv::Mat img_frame = async_cam.get_ImageBuffer();
        
        //Check if frame valid
        bool has_frame = ((img_frame.rows > 0) && (img_frame.cols > 0))?true:false;
        
        //If valid frame, record and display it..
        if (has_frame)  
        {
            //if recording, capture frames and flash "recording" on screen.
            if (is_recording)
            {
                camcorder.add_frame(img_frame);
                cv::Scalar color_red = cv::Scalar(0,0,0xff);

                //show "recording" from half delay frames
                if (frame_flash > wait_detalay/2)
                {
                    cv::circle(img_frame, cv::Point(28,28), 25, color_red, CV_FILLED, 8,0);
                    cv::putText(img_frame, "Recording...", cv::Point(65,50), CV_FONT_HERSHEY_COMPLEX, 2, color_red, 2);
                }
                
                //count up to delay and reset to 0
                if (frame_flash >= wait_detalay)
                    frame_flash = 0;
                else
                    frame_flash ++;
            }
            
            //show opencv window
            cv::imshow(window_name, img_frame); 

        }
        
        //wait 33ms for key event.
        int x = cv::waitKey(wait_detalay);
        
        //check which key was pressed..
        if (x == 32) //space bar
        {
            if (has_frame && !is_recording) 
            {
                //enable recording with default video file name
                camcorder.start_recorder("ip-camera.mp4", 1000/wait_detalay, cv::Size(img_frame.cols, img_frame.rows));
                is_recording = true;
            }
            else
            { 
                //stop recorder..
                camcorder.stop_recorder();
                is_recording = false;
            }
        
        }
        else if (x == 27) //escape
        {
            break;
        }
    }


    //stop recorder..
    if (is_recording) 
        camcorder.stop_recorder();

    //stop cam
    async_cam.thread_stopwait();
    
}