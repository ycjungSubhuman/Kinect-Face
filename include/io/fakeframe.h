#pragma once

#include "io/devicecloud.h"
#include "io/png.h"
#include "type.h"

namespace {
    namespace fs = std::experimental::filesystem;
    using namespace telef::types;
}

namespace telef::io {
    /**
     * Fake frame that is recoreded once and played back later
     *
     * Used for feeding RGB-D dataset or running programs for same input multiple times
     **/
    class FakeFrame {
    private:
        boost::shared_ptr<DeviceCloud> dc;
        ImagePtrT image;

    public:
        FakeFrame(boost::shared_ptr<DeviceCloudConstT> dc, ImagePtrT image);

        boost::shared_ptr<DeviceCloud> getDeviceCloud();
        ImagePtrT getImage();
        pcl::io::DepthImage::ConstPtr getDepthImage();

        /** Load from existing file */
        FakeFrame(fs::path p);
        /** Save as a file */
        void save(fs::path p);
    };
}
