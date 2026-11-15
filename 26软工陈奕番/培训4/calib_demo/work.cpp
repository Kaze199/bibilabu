#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp> // 引入相机标定和图像去畸变相关接口
#include <cctype>              // 引入 tolower，用于将文件扩展名转换为小写
#include <filesystem>          // C++17 文件系统接口，用于遍历图片目录并处理路径
#include <iostream>
#include <vector>
using namespace std;
using namespace cv;
namespace fs = std::filesystem; 

// ---------- 标定板参数（唯一需要你改的三个值）---------- // 
static const int kCols = 10;          // 棋盘格内角点的列数
static const int kRows = 7;           // 棋盘格内角点的行数
static const double kSquareMm = 15.0; // 每个方格边长的实际长度，单位为毫米

static const cv::Scalar kPalette[7] = {
    {60, 60, 255},  // 红
    {60, 150, 255}, // 橙
    {60, 220, 255}, // 黄
    {80, 220, 120}, // 绿
    {230, 220, 80}, // 青
    {255, 160, 70}, // 蓝
    {255, 90, 220}, // 品红
};

int main()
{                              
    string dir = "cam_mix_img"; 
    if (!fs::exists(dir) || !fs::is_directory(dir))
    {                                                                     
        cerr << "图片目录不存在: " << dir << "（请从项目目录运行程序）\n";
        return 1;                                                          
    } 

    // ---------- 1. 收集图片 ---------- //从指定目录中筛出扩展名为 JPG 或 PNG 的图片
    vector<fs::path> files;
    for (const auto &e : fs::directory_iterator(dir))
    {
        if (!e.is_regular_file())
            continue;
        string ext = e.path().extension().string();
        for (auto &ch : ext)
            ch = tolower(ch); // 遍历字符

        if (ext == ".jpg" || ext == ".png")
            files.push_back(e.path()); // files.push_back(值);把「值」追加到数组末尾，长度 +1
    }

    // ---------- 2. 构造棋盘格的 3D 世界坐标---------- //

    vector<cv::Point3f> objectPointsOne; // 单张标定图共用的三维点模板，先把数据写进数组
    for (int r = 0; r < kRows; ++r)
        for (int c = 0; c < kCols; ++c)
            objectPointsOne.emplace_back(c * kSquareMm, r * kSquareMm, 0.0f);

    // ---------- 3. 逐张图找角点 ---------- //
    vector<vector<Point3f>> objectPoints;
    vector<vector<Point2f>> imagePoints;
    Size imageSize;
    vector<string> failedNames, goodNames;
    for (const auto &f : files)
    {
        Mat gray = imread(f.string(), IMREAD_GRAYSCALE); // imread只能识别path和string作为路径参数，所以要转化
        if (gray.empty())
        {
            cerr << "读取失败" << f << endl;
            continue;
        }

        vector<Point2f> corners;
        bool ok = findChessboardCorners(gray, Size(kCols, kRows),
                                        corners, CALIB_CB_ADAPTIVE_THRESH | CALIB_CB_NORMALIZE_IMAGE);

        if (!ok)
        {
            failedNames.push_back(f.filename().string());
            continue;
        }
        cornerSubPix(gray, corners, Size(11, 11), Size(-1, -1), // 亚像素精修
                     TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.001));
      
        if (imagePoints.empty()) // 只保存第一张成功检测的图片，画出棋盘格角点
        {
            Mat show;
            cvtColor(gray, show, COLOR_GRAY2BGR);
            const Scalar color(0, 0, 255);
            const int kColorCount = (int)(sizeof(kPalette) / sizeof(kPalette[0]));
            for (int i = 0; i < (int)corners.size(); ++i)
            {
                const Scalar &color = kPalette[(i / kCols) % kColorCount];
                if (i % kCols != 0)
                    line(show, corners[i - 1], corners[i], color, 1, LINE_AA);
                circle(show, corners[i], 4, color, -1, LINE_AA);
                putText(show, to_string(i + 1), corners[i] + Point2f(4, -4),
                        FONT_HERSHEY_SIMPLEX, 0.38, color, 1, LINE_AA);
            }
            imwrite("corners.jpg", show);
        }

        objectPoints.push_back(objectPointsOne); // 与 corners 一一对应
        imagePoints.push_back(corners);
        goodNames.push_back(f.filename().string()); // 同一位置记录来源

        
        imageSize = gray.size();  //calibrateCamera 的签名要求传入图像尺寸
    }

    cout << "发现图片:" << files.size() << endl;
    cout << "成功检出角点的图片: " << imagePoints.size() << " 张\n";
    if (imagePoints.size() < 6)
    {
        cout << "有效图片少于 6 张，请补充图片或检查棋盘格参数\n";
        return 1;
    }

    // ---------- 4. 标定（一行算出 K 和 D）---------- //
    Mat cameraMatrix, distCoeffs;
    vector<Mat> rvecs, tvecs; // 声明旋转和平移向量
    double rms = calibrateCamera(objectPoints, imagePoints, imageSize, cameraMatrix, distCoeffs, rvecs, tvecs);

    cout << "重投影误差 RMS = " << rms << " 像素" << endl;
    cout << "内参矩阵 K =" << "\n"
         << cameraMatrix << endl;
    cout << "畸变参数 D =\n"
         << distCoeffs;

    // ---------- 5. 矫正图片 ---------- // 
    
    // Mat before = imread(files[0].string()); // 读取文件列表中的第一张彩色原图；它未必是前面成功检测角点的图片。

    fs::path sourcePath = fs::path(dir) / goodNames.front();
    Mat before = imread(sourcePath.string());

    if (before.empty())
    {
        cerr << "矫正用原图读取失败: " << sourcePath << endl;
        return 1;
    }

    Mat after, com;
    undistort(before, after, cameraMatrix, distCoeffs);
    hconcat(before, after, com);
    imwrite("undistort_before.jpg", before);
    imwrite("undistort_after.jpg", after);
    imwrite("undistort_compare.jpg", com);

    cout << "\n已输出 corners.jpg、undistort_before.jpg、undistort_after.jpg\n";

    return 0;
}