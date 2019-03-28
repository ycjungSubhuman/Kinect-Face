#pragma once

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <dlib/data_io.h>
#include <dlib/gui_widgets.h>
#include <dlib/image_processing.h>
#include <dlib/image_transforms.h>
#include <dlib/opencv/cv_image.h>

#include "face.h"
#include "feature/face.h"
#include "io/frontend.h"
#include "util/eigen_pcl.h"

namespace {
using namespace telef::feature;
}

namespace telef::io {
/** Visualize Pointcloud through PCL Visualizer */
class FaceDetectFrontEnd : public FrontEnd<telef::feature::FeatureDetectSuite> {
private:
  dlib::image_window win;

public:
  using InputPtrT = const boost::shared_ptr<telef::feature::FeatureDetectSuite>;

  void process(InputPtrT input) override {
    // Convert PCL image to dlib image

    dlib::matrix<dlib::rgb_pixel> img;
    auto pclImage = input->deviceInput->rawImage;

    auto matImg = cv::Mat(pclImage->getHeight(), pclImage->getWidth(), CV_8UC3);
    pclImage->fillRGB(matImg.cols, matImg.rows, matImg.data, matImg.step);
    cv::cvtColor(matImg, matImg, CV_RGB2BGR);
    dlib::cv_image<dlib::bgr_pixel> cvImg(matImg);
    dlib::assign_image(img, cvImg);

    win.clear_overlay();
    win.set_image(img);

    win.add_overlay(input->feature->boundingBox.getRect());
  }
};

/** Visualize Pointcloud through PCL Visualizer */
class Feature2DDetectFrontEnd
    : public FrontEnd<telef::feature::FeatureDetectSuite> {
private:
  dlib::image_window win;

public:
  using InputPtrT = const boost::shared_ptr<telef::feature::FeatureDetectSuite>;

  void process(InputPtrT input) override {
    // Convert PCL image to dlib image

    dlib::matrix<dlib::rgb_pixel> img;
    auto pclImage = input->deviceInput->rawImage;

    auto matImg = cv::Mat(pclImage->getHeight(), pclImage->getWidth(), CV_8UC3);
    pclImage->fillRGB(matImg.cols, matImg.rows, matImg.data, matImg.step);
    cv::cvtColor(matImg, matImg, CV_RGB2BGR);

    dlib::cv_image<dlib::bgr_pixel> cvImg(matImg);
    dlib::assign_image(img, cvImg);

    win.clear_overlay();
    win.set_image(img);

    auto featurePts = input->feature->points;
    for (size_t i = 0, size = featurePts.rows(); i < size; i++) {
      int radius = 1;
      dlib::point point(featurePts(i, 0), featurePts(i, 1));
      win.add_overlay(dlib::image_window::overlay_circle(
          point, radius, dlib::rgb_pixel(0, 255, 0)));
    }
  }
};

/** Visualize Pointcloud through PCL Visualizer */
class FeatureDetectFrontEnd : public FrontEnd<telef::feature::FittingSuite> {
private:
  using InputPtrT = const boost::shared_ptr<telef::feature::FittingSuite>;

  std::unique_ptr<vis::PCLVisualizer> visualizer;

public:
  void process(InputPtrT input) override {
    // Display Features in 3D
    auto lmks = input->landmark3d;

    if (!visualizer) {
      visualizer = std::make_unique<vis::PCLVisualizer>();
      visualizer->setBackgroundColor(0, 0, 0);
    }

    visualizer->spinOnce();
    if (!visualizer->updatePointCloud(lmks)) {
      visualizer->addPointCloud(lmks);
      visualizer->setPosition(0, 0);
      visualizer->setPointCloudRenderingProperties(
          pcl::visualization::PCL_VISUALIZER_POINT_SIZE, 5);
      // visualizer->setSize (cloud->width, cloud->height);
      visualizer->initCameraParameters();
    }
  }
};
} // namespace telef::io