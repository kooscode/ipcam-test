#include <stdio.h>
#include <iostream>
#include <ctime>

//opencv
#include <opencv2/imgproc/imgproc_c.h>

//libterraclear
#include "libterraclear/src/camera_base.hpp"
#include "libterraclear/src/camera_file.hpp"
#include "libterraclear/src/camera_usb.hpp"
#include "libterraclear/src/camera_async.hpp"
#include "libterraclear/src/camera_recorder.hpp"
#include "libterraclear/src/detector_motion.hpp"
#include "libterraclear/src/stopwatch.hpp"

namespace tc = terraclear;

int main(int argc, char** argv) 
{
    if (argc < 2)
    {
        std::cout << "Requires camera index or path to file or gstreamer URI" << std::endl;
        std::cout << "\tUSB Example: ipcam-test 0" << std::endl;
        std::cout << "\tFile Example: ipcam-test /home/foo/bar.mp4" << std::endl;
        std::cout << "\tGST Example: ipcam-test \"rtsp://USR:PWD@IP:80/cam/realmonitor?channel=1&subtype=0\"" << std::endl << std::endl;  
        return -1;
    }
    
    //Camera index or path
    std::string video_str = argv[1];

    //camera ptr.
    tc::camera_base* ptr_sec_cam = nullptr;
    try 
    {
       //open from USB 
        if (std::isdigit(video_str.at(0)))
            ptr_sec_cam = new tc::camera_usb(atoi(video_str.c_str()));
        else 
            ptr_sec_cam = new tc::camera_file(video_str);

    }
    catch(const std::runtime_error& re)
    {
        std::cout << "\n\tCould not open video feed: ";
        std::cout << re.what() << std::endl;
        return -1;
    }        
    catch (std::exception e)
    {
        std::cout << "\n\tCould not open video feed: ";
        std::cout << e.what() << std::endl;
        return -1;
    }   
    
    //init OpenCV window
    char window_name[] = "cctv";
    cv::namedWindow(window_name, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO);// | cv::WINDOW_AUTOSIZE);
 
    //async camera thread
    tc::camera_async async_cam(ptr_sec_cam);
    async_cam.thread_start("async-cam");
    
    //init start camcorder
    tc::camera_recorder camcorder;
    
    //recording & motion detection recording.. 
    bool is_recording = false;
    int wait_detalay = 33;
    int frame_flash = 0;

    cv::Scalar color_red = cv::Scalar(0,0,0xff);

    std::cout << "Loading camera";
    cv::Mat img_frame;
    
    //wait for first frame from Camera
    while (!img_frame.data) 
    {
        std::cout << "." << std::flush;
        img_frame = async_cam.get_ImageBuffer();
        usleep(250000);        
    }
    
    std::cout << std::endl;
    
    //setup motion detector..
    tc::detector_motion motion(img_frame);
    motion.set_motion_threshold(800);
    tc::stopwatch motion_sw;
    uint32_t motion_timeout = 10;
    bool is_motion = false;
    
    //display loop..
    for (;;)
    {
        //get frame
        async_cam.get_ImageBuffer().copyTo(img_frame);
   
        //Check if frame valid
        bool has_frame = ((img_frame.rows > 0) && (img_frame.cols > 0)) ? true : false;
        
        //If valid frame, record and display it..
        if (has_frame)  
        {
            //check for motion..
            std::vector<tc::bounding_box> motion_objects  = motion.detect_objects();
            if (motion_objects.size() > 0)
            {
                //enable and reset motion recording timer
                is_motion = true;
                motion_sw.reset();
                motion_sw.start();
                
                //start or resume auto recording..
                if (!is_recording) 
                {
                    //enable motion recording..
                    camcorder.start_recorder("ip-camera-motion.mp4", 1000/wait_detalay, cv::Size(img_frame.cols, img_frame.rows));
                    is_recording = true;
                }
                else if (camcorder.ispaused())
                {
                    //resume if already recording..
                    camcorder.resume_recorder();
                }
                
            }
            else if ((is_motion) && (motion_sw.get_elapsed_s() > motion_timeout))
            {
                //pause auto recording..
                if (is_recording) 
                {
                    camcorder.pause_recorder();
                }
                
                //stop & reset
                is_motion = false;     
                motion_sw.stop();
                motion_sw.reset();
            }
            
            //current date/time
            time_t now = time(0);
            tm *ltm = localtime(&now);
            
            //construct time/date string
            std::stringstream sstr;
            sstr << 1900 + ltm->tm_year << "-" << std::setfill('0') << std::setw(2) << 1 + ltm->tm_mon << "-";
            sstr << std::setfill('0') << std::setw(2) << ltm->tm_mday << " ";
            sstr << std::setfill('0') << std::setw(2) << ltm->tm_hour << ":"; 
            sstr << std::setfill('0') << std::setw(2) << ltm->tm_min << ":" << std::setfill('0') << std::setw(2) << ltm->tm_sec;

            cv::putText(img_frame, sstr.str(), cv::Point(5, img_frame.rows - 10), CV_FONT_HERSHEY_COMPLEX, 1.125, color_red, 2);

            //if recording, capture frames and flash "recording" on screen.
            if (is_recording)
            {
                camcorder.add_frame(img_frame);

                //show "recording" from half delay frames
                if (frame_flash > wait_detalay/2)
                {
                    sstr.str("");
                    if (camcorder.ispaused())
                        sstr << "PAUSED";
                    else
                        sstr << "RECORDING";
                        
                    if (is_motion)
                        sstr << " [MOTION]";
                    
                    cv::circle(img_frame, cv::Point(28,28), 20, color_red, CV_FILLED, 4,0);
                    cv::putText(img_frame, sstr.str(), cv::Point(65,42), CV_FONT_HERSHEY_COMPLEX, 1.25, color_red, 2);
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
        if (x == 27) //escape
        {
            break;
        }
        else if (x == 32) //space bar
        {
            if (has_frame && !is_recording) 
            {
                //enable recording with default video file name
                camcorder.start_recorder("ip-camera-recording.mp4", 1000/wait_detalay, cv::Size(img_frame.cols, img_frame.rows));
                is_recording = true;
            }
            else
            { 
                //stop recorder..
                camcorder.stop_recorder();
                is_recording = false;
            }
        
        }
        
    }


    //stop recorder..
    if (is_recording) 
        camcorder.stop_recorder();

    //stop cam
    async_cam.thread_stopwait();
    
    //free camera
    delete ptr_sec_cam;
    
}