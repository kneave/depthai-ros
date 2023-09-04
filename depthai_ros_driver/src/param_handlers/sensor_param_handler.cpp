#include "depthai_ros_driver/param_handlers/sensor_param_handler.hpp"

#include "depthai-shared/common/CameraBoardSocket.hpp"
#include "depthai-shared/properties/ColorCameraProperties.hpp"
#include "depthai/pipeline/node/ColorCamera.hpp"
#include "depthai/pipeline/node/MonoCamera.hpp"
#include "depthai_ros_driver/dai_nodes/sensors/sensor_helpers.hpp"
#include "depthai_ros_driver/utils.hpp"
#include "ros/node_handle.h"

namespace depthai_ros_driver {
namespace param_handlers {
SensorParamHandler::SensorParamHandler(ros::NodeHandle node, const std::string& name) : BaseParamHandler(node, name) {
    fSyncModeMap = {
        {"OFF", dai::CameraControl::FrameSyncMode::OFF},
        {"OUTPUT", dai::CameraControl::FrameSyncMode::OUTPUT},
        {"INPUT", dai::CameraControl::FrameSyncMode::INPUT},
    };
    cameraImageOrientationMap = {
        {"NORMAL", dai::CameraImageOrientation::NORMAL},
        {"ROTATE_180_DEG", dai::CameraImageOrientation::ROTATE_180_DEG},
        {"AUTO", dai::CameraImageOrientation::AUTO},
        {"HORIZONTAL_MIRROR", dai::CameraImageOrientation::HORIZONTAL_MIRROR},
        {"VERTICAL_FLIP", dai::CameraImageOrientation::VERTICAL_FLIP},
    };
}
SensorParamHandler::~SensorParamHandler() = default;

void SensorParamHandler::declareParams(std::shared_ptr<dai::node::MonoCamera> monoCam,
                                       dai::CameraBoardSocket socket,
                                       dai_nodes::sensor_helpers::ImageSensor,
                                       bool publish) {
    monoResolutionMap = {
        {"400", dai::MonoCameraProperties::SensorResolution::THE_400_P},
        {"480", dai::MonoCameraProperties::SensorResolution::THE_480_P},
        {"720", dai::MonoCameraProperties::SensorResolution::THE_720_P},
        {"800", dai::MonoCameraProperties::SensorResolution::THE_800_P},
        {"1200", dai::MonoCameraProperties::SensorResolution::THE_1200_P},
    };
    getParam<int>("i_max_q_size", 30);
    getParam<bool>("i_publish_topic", publish);
    getParam<int>("i_board_socket_id", static_cast<int>(socket));

    monoCam->setBoardSocket(socket);
    monoCam->setFps(getParam<double>("i_fps", 30.0));

    monoCam->setResolution(utils::getValFromMap(getParam<std::string>("i_resolution", "720"), monoResolutionMap));
    getParam<int>("i_width");
    getParam<int>("i_height");
    size_t iso = getParam<int>("r_iso", 800);
    size_t exposure = getParam<int>("r_exposure", 1000);

    if(getParam<bool>("r_set_man_exposure", false)) {
        monoCam->initialControl.setManualExposure(exposure, iso);
    }
}

void SensorParamHandler::declareParams(std::shared_ptr<dai::node::ColorCamera> colorCam,
                                       dai::CameraBoardSocket socket,
                                       dai_nodes::sensor_helpers::ImageSensor sensor,
                                       bool publish) {
    rgbResolutionMap = {{"720p", dai::ColorCameraProperties::SensorResolution::THE_720_P},
                        {"1080p", dai::ColorCameraProperties::SensorResolution::THE_1080_P},
                        {"4k", dai::ColorCameraProperties::SensorResolution::THE_4_K},
                        {"12mp", dai::ColorCameraProperties::SensorResolution::THE_12_MP},
                        {"13mp", dai::ColorCameraProperties::SensorResolution::THE_13_MP},
                        {"800p", dai::ColorCameraProperties::SensorResolution::THE_800_P},
                        {"1200p", dai::ColorCameraProperties::SensorResolution::THE_1200_P},
                        {"5MP", dai::ColorCameraProperties::SensorResolution::THE_5_MP},
                        {"4000x3000", dai::ColorCameraProperties::SensorResolution::THE_4000X3000},
                        {"5312X6000", dai::ColorCameraProperties::SensorResolution::THE_5312X6000},
                        {"48mp", dai::ColorCameraProperties::SensorResolution::THE_48_MP},
                        {"1440X1080", dai::ColorCameraProperties::SensorResolution::THE_1440X1080}};

    getParam<int>("i_max_q_size", 30);
    getParam<bool>("i_publish_topic", publish);
    getParam<bool>("i_enable_preview", false);
    getParam<int>("i_board_socket_id", static_cast<int>(socket));

    colorCam->setBoardSocket(socket);
    colorCam->setFps(getParam<int>("i_fps", 30.0));
    int preview_size = getParam<int>("i_preview_size", 300);
    int preview_width = getParam<int>("i_preview_width", preview_size);
    int preview_height = getParam<int>("i_preview_height", preview_size);
    // if resolution not in allowed resolutions, use default
    auto resString = getParam<std::string>("i_resolution", sensor.defaultResolution);

    if(std::find(sensor.allowedResolutions.begin(), sensor.allowedResolutions.end(), resString) == sensor.allowedResolutions.end()) {
        ROS_WARN(
            "Resolution %s not supported by sensor %s. Using default resolution %s", resString.c_str(), sensor.name.c_str(), sensor.defaultResolution.c_str());
        resString = sensor.defaultResolution;
    }

    int width = colorCam->getResolutionWidth();
    int height = colorCam->getResolutionHeight();
    colorCam->setResolution(utils::getValFromMap(resString, rgbResolutionMap));

    colorCam->setInterleaved(getParam<bool>("i_interleaved", false));
    if(getParam<bool>("i_set_isp_scale", true)) {
        int num = getParam<int>("i_isp_num", 2);
        int den = getParam<int>("i_isp_den", 3);
        width = (width * num + den - 1) / den;
        height = (height * num + den - 1) / den;
        if(width < getParam<int>("i_preview_size") || height < getParam<int>("i_preview_size")) {
            throw std::runtime_error("ISP image size lower than preview size! Adjust preview size accordingly");
        }
        colorCam->setIspScale(num, den);
        if(width % 16 != 0 && height % 16 != 0) {
            std::stringstream err_stream;
            err_stream << "ISP scaling with num: " << num << " and den: " << den << " results in width: " << width << " and height: " << height;
            err_stream << " which are not divisible by 16.\n";
            err_stream << "This will result in errors when aligning stereo to RGB. To fix that, either adjust i_num and i_den values";
            err_stream << " or set i_output_isp parameter to false and set i_width and i_height parameters accordingly.";
            ROS_ERROR(err_stream.str().c_str());
        }
    }
    if(getParam<bool>("i_output_isp")) {
        setParam<int>("i_width", width);
        setParam<int>("i_height", height);
    }
    colorCam->setVideoSize(getParam<int>("i_width", width), getParam<int>("i_height", height));

    colorCam->setPreviewKeepAspectRatio(getParam<bool>("i_keep_preview_aspect_ratio", true));
    size_t iso = getParam<int>("r_iso", 800);
    size_t exposure = getParam<int>("r_exposure", 20000);
    size_t whitebalance = getParam<int>("r_whitebalance", 3300);
    size_t focus = getParam<int>("r_focus", 1);
    if(getParam<bool>("r_set_man_focus", false)) {
        colorCam->initialControl.setManualFocus(focus);
    }
    if(getParam<bool>("r_set_man_exposure", false)) {
        colorCam->initialControl.setManualExposure(exposure, iso);
    }
    if(getParam<bool>("r_set_man_whitebalance", false)) {
        colorCam->initialControl.setManualWhiteBalance(whitebalance);
    }

    if(declareAndLogParam<bool>("i_fsync_continuous", false)) {
        colorCam->initialControl.setFrameSyncMode(utils::getValFromMap(declareAndLogParam<std::string>("i_fsync_mode", "INPUT"), fSyncModeMap));
    }
    if(declareAndLogParam<bool>("i_fsync_trigger", false)) {
        colorCam->initialControl.setExternalTrigger(declareAndLogParam<int>("i_num_frames_burst", 1), declareAndLogParam<int>("i_num_frames_discard", 0));
    }

    if(declareAndLogParam<bool>("i_set_isp3a_fps", false)) {
        colorCam->setIsp3aFps(declareAndLogParam<int>("i_isp3a_fps", 10));
    }
    colorCam->setImageOrientation(utils::getValFromMap(declareAndLogParam<std::string>("i_sensor_img_orientation", "NORMAL"), cameraImageOrientationMap));
}
dai::CameraControl SensorParamHandler::setRuntimeParams(parametersConfig& config) {
    dai::CameraControl ctrl;

    if(getName() == "rgb") {
        if(config.rgb_r_set_man_exposure) {
            ctrl.setManualExposure(config.rgb_r_exposure, config.rgb_r_iso);
        } else {
            ctrl.setAutoExposureEnable();
        }

        if(config.rgb_r_set_man_focus) {
            ctrl.setManualFocus(config.rgb_r_focus);
        } else {
            ctrl.setAutoFocusMode(dai::CameraControl::AutoFocusMode::CONTINUOUS_PICTURE);
        }
        if(config.rgb_r_set_man_whitebalance) {
            ctrl.setManualWhiteBalance(config.rgb_r_whitebalance);
        } else {
            ctrl.setAutoWhiteBalanceMode(dai::CameraControl::AutoWhiteBalanceMode::AUTO);
        }
    } else if(getName() == "left") {
        if(config.left_r_set_man_exposure) {
            ctrl.setManualExposure(config.left_r_exposure, config.left_r_iso);
        } else {
            ctrl.setAutoExposureEnable();
        }

        if(config.left_r_set_man_focus) {
            ctrl.setManualFocus(config.left_r_focus);
        } else {
            ctrl.setAutoFocusMode(dai::CameraControl::AutoFocusMode::CONTINUOUS_PICTURE);
        }
        if(config.left_r_set_man_whitebalance) {
            ctrl.setManualWhiteBalance(config.left_r_whitebalance);
        } else {
            ctrl.setAutoWhiteBalanceMode(dai::CameraControl::AutoWhiteBalanceMode::AUTO);
        }
    } else if(getName() == "right") {
        if(config.right_r_set_man_exposure) {
            ctrl.setManualExposure(config.right_r_exposure, config.right_r_iso);
        } else {
            ctrl.setAutoExposureEnable();
        }

        if(config.right_r_set_man_focus) {
            ctrl.setManualFocus(config.right_r_focus);
        } else {
            ctrl.setAutoFocusMode(dai::CameraControl::AutoFocusMode::CONTINUOUS_PICTURE);
        }
        if(config.right_r_set_man_whitebalance) {
            ctrl.setManualWhiteBalance(config.right_r_whitebalance);
        } else {
            ctrl.setAutoWhiteBalanceMode(dai::CameraControl::AutoWhiteBalanceMode::AUTO);
        }
    }

    return ctrl;
}

}  // namespace param_handlers
}  // namespace depthai_ros_driver