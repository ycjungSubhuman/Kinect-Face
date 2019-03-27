

#include <iostream>
#include <dlib/dnn.h>
#include <dlib/data_io.h>
#include <dlib/image_processing.h>
#include <dlib/image_transforms.h>
#include <dlib/opencv/cv_image.h>
#include <dlib/gui_widgets.h>

#include <Eigen/Dense>

#include <opencv2/core/core.hpp>
#include <opencv2/core/eigen.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/video.hpp>
#include <boost/asio.hpp>

#include <google/protobuf/util/delimited_message_util.h>

#include "util/pcl_cv.h"
#include "feature/feature_detect_pipe.h"

#include "io/socket/asio.h"
#include "messages/messages.pb.h"

using namespace std;
using namespace dlib;
using namespace telef::io;
using namespace boost::asio;


namespace telef::feature {
    /**
     * DlibFaceDetectionPipe
     * @param pretrained_model
     */
    DlibFaceDetectionPipe::DlibFaceDetectionPipe(const std::string &pretrained_model) {
        deserialize(pretrained_model) >> net;
    }

    FeatureDetectSuite::Ptr
    DlibFaceDetectionPipe::_processData(InputPtrT in) {
        matrix<rgb_pixel> img;

        // Convert PCL image to dlib image
        auto pclImage = in->rawImage;

        auto matImg = cv::Mat(pclImage->getHeight(), pclImage->getWidth(), CV_8UC3);
        pclImage->fillRGB(matImg.cols, matImg.rows, matImg.data, matImg.step);
        cv::cvtColor(matImg, matImg, CV_RGB2BGR);
        dlib::cv_image<bgr_pixel> cvImg(matImg);
        dlib::assign_image(img, cvImg);


        auto dets = net(img);
        dlib::rectangle bbox;
        double detection_confidence = 0;

        // Get best face detected
        // TODO: Handle face detection failure
        for (auto &&d : dets) {
            if (d.detection_confidence > detection_confidence) {
                detection_confidence = d.detection_confidence;
                bbox = d.rect;
            }
        }

        FeatureDetectSuite::Ptr result = boost::make_shared<FeatureDetectSuite>();

        result->deviceInput = in;
        result->feature = boost::make_shared<Feature>();
        result->feature->boundingBox.setBoundingBox(bbox);

        return result;
    }

    FeatureDetectionClientPipe::FeatureDetectionClientPipe(string address_, boost::asio::io_service &service)
        : isConnected(false), address(address_), ioService(service), clientSocket(), msg_id(0) {}

//    FeatureDetectionClientPipe::~FeatureDetectionClientPipe(){
//        // Cleanly close connection
//        disconnect();
//    };

    FeatureDetectSuite::Ptr FeatureDetectionClientPipe::_processData(FeatureDetectionClientPipe::InputPtrT in) {

        if ( clientSocket == nullptr || !isConnected ) {
            if (!connect()) {
                in->feature->points = landmarks;
                return in;
            }
        }

        // Convert PCL image to bytes
        auto pclImage = in->deviceInput->rawImage;
        std::vector<unsigned char> imgBuffer(pclImage->getDataSize());
        pclImage->fillRaw(imgBuffer.data());

        LmkReq reqMsg;
        auto hdr = reqMsg.mutable_hdr();
        hdr->set_id(++msg_id);
        hdr->set_width(pclImage->getHeight());
        hdr->set_height(pclImage->getWidth());
        hdr->set_channels(3); // TODO: What if we have grey or 4-ch image??

        auto imgData = reqMsg.mutable_data();
        imgData->set_buffer(imgBuffer.data(), pclImage->getDataSize());

        bool msgSent = send(reqMsg);

        LmkRsp rspMsg;
        if (msgSent && recv(rspMsg)) {

//            cout << "Lmk Size: " << rspMsg.dim().shape().size() << endl;
//            cout << "Lmk Dim: " << rspMsg.dim().shape()[0] << ", " << rspMsg.dim().shape()[1] << endl;

            auto data = rspMsg.data();

            //construct and populate the matrix
            cv::Mat m(rspMsg.dim().shape()[0], rspMsg.dim().shape()[1],
                    CV_32F, data.data());
//            cout << "M_lmks = "  << m << endl << endl;

            // make depth negative??
            m.col(2) *= -1.0f;

            cv::cv2eigen(m.t(), landmarks);
//            cout << "eigen_Lmks:\n" << landmarks << endl;
        }

        in->feature->points = landmarks;
        return in;
    }

    bool FeatureDetectionClientPipe::send(google::protobuf::MessageLite &msg){
        if (isConnected != true) return false;

        try {
            telef::io::socket::AsioOutputStream aos(*clientSocket);
            google::protobuf::io::CopyingOutputStreamAdaptor cos_adp(&aos);
            google::protobuf::io::CodedOutputStream cos(&cos_adp);

            google::protobuf::util::SerializeDelimitedToCodedStream(msg, &cos);
            cout << "Sending Message ID: " << msg_id << endl;
        }
        catch(exception& e) {
            std::cerr << "Error while sending message: " << e.what() << endl;
            disconnect();
            return false;
        }

        return true;
    }

    bool FeatureDetectionClientPipe::recv(google::protobuf::MessageLite &msg){
        if (isConnected != true) return false;
        try {
            telef::io::socket::AsioInputStream ais(*clientSocket);
            google::protobuf::io::CopyingInputStreamAdaptor cis_adp(&ais);
            google::protobuf::io::CodedInputStream cis(&cis_adp);
            bool parseStatus = false;

            google::protobuf::util::ParseDelimitedFromCodedStream(&msg, &cis, &parseStatus);
        }
        catch(exception& e) {
            std::cerr << "Error while receiving message: " << e.what() << endl;
            disconnect();
            return false;
        }

        return true;
    }

    bool FeatureDetectionClientPipe::connect(){
        isConnected = false;

        clientSocket = make_shared<SocketT>(ioService);

        try {
            clientSocket->connect(boost::asio::local::stream_protocol::endpoint(address));
            isConnected = true;

            cout << "Connected to server" << endl;
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << "Failed to connect to server..." << std::endl;
            disconnect();
        }

        return isConnected;
    }

    void FeatureDetectionClientPipe::disconnect(){
        isConnected = false;

        try {
	  //clientSocket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
	  //clientSocket->close();
            cout << "Disconnected..." << endl;
        }
        catch (std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            std::cerr << "Failed to cleanly disconnect to server..." << std::endl;
        }

        clientSocket.reset();
    }
}
