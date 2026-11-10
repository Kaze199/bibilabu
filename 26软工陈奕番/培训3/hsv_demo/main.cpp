#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
// using namespace cv;

int main() {
    cv::Mat image=cv::imread("assets/armor.jpg");

    if(image.empty()){
        std::cerr << "无法识别到图片" << std::endl;
        return 1;
    }
    cv::Mat hsvImage;
    cv::cvtColor(image,hsvImage,cv::COLOR_BGR2HSV);

    std::vector<cv::Mat> hsvchannels;
    cv::split(hsvImage,hsvchannels);

    cv::imshow("H channel", hsvchannels[0]);
    cv::imshow("S channel", hsvchannels[1]);
    cv::imshow("V channel", hsvchannels[2]);

    cv::Mat redmasklow;
    cv::Mat redmask;
    cv::Mat redmaskhigh;

    cv::inRange(hsvImage, cv::Scalar(0,100,80),
    cv::Scalar(10,255,255),redmasklow);

    cv::inRange(hsvImage, cv::Scalar(160, 100, 80),
    cv::Scalar(179, 255, 255), redmaskhigh);

    cv::bitwise_or(redmasklow, redmaskhigh, redmask);

    cv::imshow("Red mask", redmask);
    cv::imwrite("red_mask.png", redmask);
    cv::waitKey(0);

    return 0;

}