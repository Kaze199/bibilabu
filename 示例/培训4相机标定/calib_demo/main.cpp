// ============================================================
// 培训4 示例：相机内参标定 + 畸变矫正
//
// 用法：
//   calib_demo [图片目录] [--show]
//
//   图片目录   默认 "cam_mix_img"，里面放 img0.jpg ...（从 cam_mix_img.zip 解压）
//   --show     额外弹出窗口显示（默认只写文件，方便在服务器上跑）
//
// 输出（写在当前工作目录）：
//   corners.jpg    第一张成功检出的图，把 70 个角点编号画出来
//   undistort_before.jpg      矫正前的原始图
//   undistort_after.jpg      畸变矫正之后的图
//   undistort_compare.jpg    上面两张左右拼在一起
//
// 标定板：11 列 × 8 行黑白方格（内角点 10 × 7），每格 15 mm
// ============================================================
#include <opencv2/opencv.hpp>

// projectPoints 在 OpenCV 4.x 里属于 calib3d，OpenCV 5 挪到了 geometry/3d。
// 两行都写上，Ubuntu 22.04 的 4.5.4 和本机 5.x 都能编。
#if __has_include(<opencv2/geometry/3d.hpp>)
#  include <opencv2/geometry/3d.hpp>
#else
#  include <opencv2/calib3d.hpp>
#endif

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------- 标定板参数 ----------
static const int    kCols      = 10;    // 内角点列数（注意：不是方格数 11）
static const int    kRows      = 7;     // 内角点行数（注意：不是方格数 8）
static const double kSquareMm  = 15.0;  // 每格边长 15 mm（不影响内参，只影响平移量）

static const cv::Scalar kPalette[7] = {
    {  60,  60, 255},   // 红
    {  60, 150, 255},   // 橙
    {  60, 220, 255},   // 黄
    {  80, 220, 120},   // 绿
    { 230, 220,  80},   // 青
    { 255, 160,  70},   // 蓝
    { 255,  90, 220},   // 品红
};

// 把角点按 7 行 × 10 列画出来：行内连线、每个点画圆、标 1~70 的编号
static void drawNumberedCorners(cv::Mat& canvas, const std::vector<cv::Point2f>& corners) {
    for (int r = 0; r < kRows; ++r) {
        const cv::Scalar color = kPalette[r % 7];
        for (int c = 0; c < kCols; ++c) {
            const int idx = r * kCols + c;
            if (idx >= static_cast<int>(corners.size())) return;
            const cv::Point p(cvRound(corners[idx].x), cvRound(corners[idx].y));
            if (c > 0) {
                const cv::Point q(cvRound(corners[idx - 1].x), cvRound(corners[idx - 1].y));
                cv::line(canvas, q, p, color, 1, cv::LINE_AA);
            }
            cv::circle(canvas, p, 3, color, -1, cv::LINE_AA);
            cv::putText(canvas, std::to_string(idx + 1), p + cv::Point(4, -4),
                        cv::FONT_HERSHEY_SIMPLEX, 0.38, color, 1, cv::LINE_AA);
        }
    }
}

// 左右拼两张图，并加上 before / after 标签
static cv::Mat sideBySide(const cv::Mat& a, const cv::Mat& b) {
    cv::Mat out(a.rows, a.cols * 2 + 8, a.type(), cv::Scalar(30, 30, 30));
    a.copyTo(out(cv::Rect(0, 0, a.cols, a.rows)));
    b.copyTo(out(cv::Rect(a.cols + 8, 0, b.cols, b.rows)));
    cv::putText(out, "before", cv::Point(10, 22), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    cv::putText(out, "after", cv::Point(a.cols + 18, 22), cv::FONT_HERSHEY_SIMPLEX, 0.6,
                cv::Scalar(0, 255, 255), 2, cv::LINE_AA);
    return out;
}

int main(int argc, char** argv) {
    std::string dir = "cam_mix_img";
    bool show = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--show") show = true;
        else dir = a;
    }

    // ---------- 1. 收集图片 ----------
    std::vector<fs::path> files;
    if (!fs::exists(dir)) {
        std::cout << "图片目录不存在: " << dir << "\n"
                  << "用法: calib_demo <图片目录> [--show]\n";
        return 1;
    }
    for (const auto& e : fs::directory_iterator(dir)) {
        if (!e.is_regular_file()) continue;
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    std::cout << "发现图片: " << files.size() << " 张，目录 " << dir << "\n";
    if (files.size() < 6) {
        std::cout << "图片太少，至少要 6 张才能标定\n";
        return 1;
    }

    // ---------- 2. 物体坐标（世界坐标系里棋盘格的 3D 点，Z 恒为 0） ----------
    std::vector<cv::Point3f> objectPointsOne;
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kCols; ++c)
            objectPointsOne.emplace_back(float(c) * float(kSquareMm),
                                         float(r) * float(kSquareMm), 0.0f);

    // ---------- 3. 逐张图找角点 ----------
    std::vector<std::vector<cv::Point3f>> objectPoints;
    std::vector<std::vector<cv::Point2f>> imagePoints;
    cv::Size imageSize;
    cv::Mat firstHitImage;
    std::vector<cv::Point2f> firstHitCorners;
    int failed = 0;

    for (const auto& f : files) {
        cv::Mat gray = cv::imread(f.string(), cv::IMREAD_GRAYSCALE);
        if (gray.empty()) { ++failed; continue; }

        std::vector<cv::Point2f> corners;
        const bool ok = cv::findChessboardCorners(
            gray, cv::Size(kCols, kRows), corners,
            cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);
        if (!ok) { ++failed; continue; }

        // 亚像素精化，能让重投影误差降一个数量级
        cv::cornerSubPix(gray, corners, cv::Size(11, 11), cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.001));

        objectPoints.push_back(objectPointsOne);
        imagePoints.push_back(corners);

        if (firstHitImage.empty()) {
            imageSize = gray.size();
            firstHitImage = cv::imread(f.string());
            firstHitCorners = corners;
        }
    }

    std::cout << "角点检出: " << imagePoints.size() << " / " << files.size()
              << " 张（" << failed << " 张检不出来，正常）\n";
    if (imagePoints.size() < 6) {
        std::cout << "有效图片不足 6 张，无法标定。检查标定板尺寸是不是写成了 11x8\n";
        return 2;
    }

    // ---------- 4. 标定 ----------
    cv::Mat cameraMatrix, distCoeffs;
    std::vector<cv::Mat> rvecs, tvecs;
    const double rms = cv::calibrateCamera(objectPoints, imagePoints, imageSize,
                                           cameraMatrix, distCoeffs, rvecs, tvecs);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "\n========== 标定结果 ==========\n";
    std::cout << "重投影误差 RMS = " << rms << " 像素\n";
    std::cout << "内参矩阵 K =\n" << cameraMatrix << "\n";
    std::cout << "畸变系数 D = " << distCoeffs.reshape(1, 1) << "\n";
    std::cout << "  （顺序 k1, k2, p1, p2, k3）\n";
    std::cout << "fx=" << cameraMatrix.at<double>(0, 0)
              << "  fy=" << cameraMatrix.at<double>(1, 1)
              << "  cx=" << cameraMatrix.at<double>(0, 2)
              << "  cy=" << cameraMatrix.at<double>(1, 2) << "\n";

    // ---------- 5. 每张图的重投影误差，挑最大的三张报出来 ----------
    std::vector<std::pair<double, int>> errs;
    for (size_t i = 0; i < objectPoints.size(); ++i) {
        std::vector<cv::Point2f> projected;
        cv::projectPoints(objectPoints[i], rvecs[i], tvecs[i], cameraMatrix, distCoeffs, projected);
        double sum = 0.0;
        for (size_t k = 0; k < projected.size(); ++k) {
            const cv::Point2f d = projected[k] - imagePoints[i][k];
            sum += d.x * d.x + d.y * d.y;
        }
        errs.emplace_back(std::sqrt(sum / projected.size()), static_cast<int>(i));
    }
    std::sort(errs.rbegin(), errs.rend());
    std::cout << "\n误差最大的三张（可以考虑剔除后再标一次）:\n";
    for (int k = 0; k < 3 && k < static_cast<int>(errs.size()); ++k)
        std::cout << "  #" << k + 1 << "  " << errs[k].first << " 像素\n";

    // ---------- 6. 写结果文件 ----------
    cv::Mat cornersCanvas = firstHitImage.clone();
    drawNumberedCorners(cornersCanvas, firstHitCorners);
    cv::imwrite("corners.jpg", cornersCanvas);
    std::cout << "\n已写出 corners.jpg\n";

    cv::Mat before = firstHitImage;
    cv::Mat after;
    // 这里把 K 本身当作"新相机矩阵"用，所以矫正图会沿用原焦距。
    // 桶形畸变（k1 为负）矫正时要从原图更靠中间的位置取样，所以看起来像"被放大了"，
    // 边缘也会被裁掉一点 —— 这是正常的。想保留完整视野可以把下面一行换成：
    //   cv::Mat newK = cv::getOptimalNewCameraMatrix(cameraMatrix, distCoeffs, before.size(), 1.0);
    //   cv::undistort(before, after, cameraMatrix, distCoeffs, newK);
    cv::undistort(before, after, cameraMatrix, distCoeffs);
    cv::imwrite("undistort_before.jpg", before);
    cv::imwrite("undistort_after.jpg", after);
    cv::imwrite("undistort_compare.jpg", sideBySide(before, after));
    std::cout << "已写出 undistort_before.jpg / undistort_after.jpg / undistort_compare.jpg\n";

    // ---------- 7. 可选：弹窗看 ----------
    if (show) {
        cv::imshow("corners", cornersCanvas);
        cv::imshow("before / after", sideBySide(before, after));
        cv::waitKey(0);
        cv::destroyAllWindows();
    }
    return 0;
}