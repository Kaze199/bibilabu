#include <opencv2/opencv.hpp>
#include <iostream>

int main() {
    cv::Mat image = cv::imread("test.jpg");

    const std::string windowName = "OpenCV 图片";

    cv::namedWindow(windowName, cv::WINDOW_NORMAL);

    cv::imshow(windowName, image);

    cv::resizeWindow(windowName, 1200, 900);

    cv::waitKey(0);


    
    return 0;
}  