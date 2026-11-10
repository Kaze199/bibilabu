#include <opencv2/opencv.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

constexpr double kMinLightbarRatio = 1.5;
constexpr double kMaxLightbarRatio = 20.0;
constexpr double kMinLightbarLength = 8.0;
constexpr double kMaxLightbarAngleError = 45.0 * CV_PI / 180.0;
constexpr double kMaxPairAngleError = 25.0 * CV_PI / 180.0;
constexpr double kMaxLightbarLengthRatio = 1.5;

struct LightbarCandidate
{
    cv::RotatedRect rect;
    cv::Point2f top;
    cv::Point2f bottom;
    double length;
    double width;
};

std::vector<LightbarCandidate> find_lightbars(const cv::Mat& redMask)
{
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(redMask.clone(), contours,
                     cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<LightbarCandidate> candidates;
    for (const auto& contour : contours)
    {
        if (cv::contourArea(contour) < 5.0)
        {
            continue;
        }

        cv::RotatedRect rect = cv::minAreaRect(contour);
        std::array<cv::Point2f, 4> corners;
        rect.points(corners.data());
        std::sort(corners.begin(), corners.end(),
                  [](const cv::Point2f& a, const cv::Point2f& b) {
                      return a.y < b.y;
                  });

        const cv::Point2f top = (corners[0] + corners[1]) * 0.5f;
        const cv::Point2f bottom = (corners[2] + corners[3]) * 0.5f;
        const double length = cv::norm(bottom - top);
        const double width =
            (cv::norm(corners[1] - corners[0]) +
             cv::norm(corners[3] - corners[2])) * 0.5;

        if (width <= 0.0 || length < kMinLightbarLength)
        {
            continue;
        }

        const double ratio = length / width;
        const double angleError =
            std::atan2(std::abs(bottom.x - top.x), std::abs(bottom.y - top.y));
        if (ratio < kMinLightbarRatio || ratio > kMaxLightbarRatio ||
            angleError > kMaxLightbarAngleError)
        {
            continue;
        }

        candidates.push_back({rect, top, bottom, length, width});
    }

    return candidates;
}

double perpendicular_error(const LightbarCandidate& left,
                           const LightbarCandidate& right)
{
    const cv::Point2f across = right.rect.center - left.rect.center;
    const cv::Point2f leftAxis = left.bottom - left.top;
    const cv::Point2f rightAxis = right.bottom - right.top;
    const double acrossLength = cv::norm(across);
    const double leftLength = cv::norm(leftAxis);
    const double rightLength = cv::norm(rightAxis);

    if (acrossLength <= 0.0 || leftLength <= 0.0 || rightLength <= 0.0)
    {
        return CV_PI;
    }

    const auto error_for_axis = [&across](const cv::Point2f& axis) {
        const double dot = std::abs(across.dot(axis));
        const double cross = std::abs(across.x * axis.y - across.y * axis.x);
        return std::atan2(dot, cross);
    };

    return std::max(error_for_axis(leftAxis), error_for_axis(rightAxis));
}

bool find_best_pair(const std::vector<LightbarCandidate>& candidates,
                    std::size_t& leftIndex, std::size_t& rightIndex)
{
    bool foundPair = false;
    double bestScore = 1e9;

    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        for (std::size_t j = i + 1; j < candidates.size(); ++j)
        {
            std::size_t left = i;
            std::size_t right = j;
            if (candidates[left].rect.center.x > candidates[right].rect.center.x)
            {
                std::swap(left, right);
            }

            const auto& a = candidates[left];
            const auto& b = candidates[right];
            const double maxLength = std::max(a.length, b.length);
            const double minLength = std::min(a.length, b.length);
            const double lengthRatio = minLength / maxLength;
            const double centerDistance = cv::norm(b.rect.center - a.rect.center);
            const double armorRatio = centerDistance / maxLength;
            const double angleError = perpendicular_error(a, b);

            if (lengthRatio < 1.0 / kMaxLightbarLengthRatio ||
                armorRatio < 1.0 || armorRatio > 5.0 ||
                angleError > kMaxPairAngleError)
            {
                continue;
            }

            const double score =
                (1.0 - lengthRatio) + angleError / kMaxPairAngleError +
                std::abs(armorRatio - 2.5) / 5.0;
            if (score < bestScore)
            {
                bestScore = score;
                leftIndex = left;
                rightIndex = right;
                foundPair = true;
            }
        }
    }

    return foundPair;
}

cv::Point to_pixel(const cv::Point2f& point)
{
    return {cvRound(point.x), cvRound(point.y)};
}

void draw_lightbar(cv::Mat& frame, const LightbarCandidate& candidate)
{
    std::array<cv::Point2f, 4> corners;
    candidate.rect.points(corners.data());

    for (std::size_t i = 0; i < corners.size(); ++i)
    {
        cv::line(frame, to_pixel(corners[i]),
                 to_pixel(corners[(i + 1) % corners.size()]),
                 cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
        cv::circle(frame, to_pixel(corners[i]), 3,
                   cv::Scalar(0, 255, 0), cv::FILLED, cv::LINE_AA);
    }
}

std::array<cv::Point2f, 2> outer_corners(const LightbarCandidate& candidate,
                                         bool chooseRightSide)
{
    std::array<cv::Point2f, 4> corners;
    candidate.rect.points(corners.data());
    std::sort(corners.begin(), corners.end(),
              [](const cv::Point2f& a, const cv::Point2f& b) {
                  return a.y < b.y;
              });

    const auto pick_outer = [chooseRightSide](const cv::Point2f& a,
                                               const cv::Point2f& b) {
        if (chooseRightSide)
        {
            return a.x > b.x ? a : b;
        }
        return a.x < b.x ? a : b;
    };

    return {pick_outer(corners[0], corners[1]),
            pick_outer(corners[2], corners[3])};
}

void draw_armor(cv::Mat& frame, const LightbarCandidate& left,
                const LightbarCandidate& right)
{
    const auto leftOuter = outer_corners(left, false);
    const auto rightOuter = outer_corners(right, true);

    const std::vector<cv::Point> armorCorners = {
        to_pixel(leftOuter[0]),
        to_pixel(rightOuter[0]),
        to_pixel(rightOuter[1]),
        to_pixel(leftOuter[1])
    };
    const std::vector<std::vector<cv::Point>> polygon = {armorCorners};

    cv::polylines(frame, polygon, true, cv::Scalar(255, 0, 0), 2, cv::LINE_AA);
    for (const auto& corner : armorCorners)
    {
        cv::circle(frame, corner, 4, cv::Scalar(0, 255, 255),
                   cv::FILLED, cv::LINE_AA);
    }
}

}  // namespace

int main()
{
    const std::string videoPath = "assets/test_clip.mp4";
    const std::string outputPath = "outputs/detect_result.mp4";

    cv::VideoCapture cap(videoPath);
    if (!cap.isOpened())
    {
        std::cerr << "无法打开视频：" << videoPath << std::endl;
        return 1;
    }

    const double inputFps = cap.get(cv::CAP_PROP_FPS);
    const double fps = inputFps > 0.0 ? inputFps : 30.0;
    const cv::Size frameSize(
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH)),
        static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT)));
    if (frameSize.width <= 0 || frameSize.height <= 0)
    {
        std::cerr << "无法获取视频画面尺寸" << std::endl;
        return 1;
    }

    std::error_code directoryError;
    std::filesystem::create_directories("outputs", directoryError);
    if (directoryError)
    {
        std::cerr << "无法创建 outputs 目录：" << directoryError.message() << std::endl;
        return 1;
    }

    cv::VideoWriter writer(
        outputPath, cv::VideoWriter::fourcc('m', 'p', '4', 'v'), fps, frameSize);
    if (!writer.isOpened())
    {
        std::cerr << "无法创建输出视频：" << outputPath << std::endl;
        return 1;
    }

    cv::namedWindow("视频播放", cv::WINDOW_NORMAL);
    cv::resizeWindow("视频播放", 1000, 700);

    cv::Mat frame;
    while (cap.read(frame))
    {
        if (frame.empty())
        {
            break;
        }

        cv::Mat hsvFrame;
        cv::cvtColor(frame, hsvFrame, cv::COLOR_BGR2HSV);

        cv::Mat maskLow;
        cv::Mat maskHigh;
        cv::Mat redMask;
        cv::inRange(hsvFrame, cv::Scalar(0, 100, 100),
                    cv::Scalar(10, 255, 255), maskLow);
        cv::inRange(hsvFrame, cv::Scalar(160, 100, 100),
                    cv::Scalar(179, 255, 255), maskHigh);
        cv::bitwise_or(maskLow, maskHigh, redMask);

        const std::vector<LightbarCandidate> candidates = find_lightbars(redMask);
        std::size_t leftIndex = 0;
        std::size_t rightIndex = 0;
        if (find_best_pair(candidates, leftIndex, rightIndex))
        {
            draw_lightbar(frame, candidates[leftIndex]);
            draw_lightbar(frame, candidates[rightIndex]);
            draw_armor(frame, candidates[leftIndex], candidates[rightIndex]);
        }

        writer.write(frame);
        cv::imshow("视频播放", frame);
        const int delay = std::max(1, cvRound(1000.0 / fps));
        if (cv::waitKey(delay) == 27)
        {
            break;
        }
    }

    writer.release();
    cap.release();
    cv::destroyAllWindows();
    std::cout << "处理后的视频已保存到：" << outputPath << std::endl;
    return 0;
}
