#include "awinda_system.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {

int findClosestUpdateRate(const XsIntArray& supportedUpdateRates, const int desiredUpdateRate) {
    if (supportedUpdateRates.empty()) {
        return 0;
    }

    int closestRate = supportedUpdateRates[0];
    int minimumDifference = std::abs(closestRate - desiredUpdateRate);

    for (const int rate : supportedUpdateRates) {
        const int difference = std::abs(rate - desiredUpdateRate);
        if (difference < minimumDifference) {
            minimumDifference = difference;
            closestRate = rate;
        }
    }
    return closestRate;
}

} // namespace

MtwCallbackRegistration::MtwCallbackRegistration(const std::vector<XsDevicePtr>& devices,
                                                 XsCallback& callback)
    : devices_(devices), callback_(&callback) {

    for (const XsDevicePtr& device : devices_) {
        device->addCallbackHandler(callback_);
    }
}

MtwCallbackRegistration::~MtwCallbackRegistration() {
    unregister();
}

MtwCallbackRegistration::MtwCallbackRegistration(MtwCallbackRegistration&& other) noexcept
    : devices_(std::move(other.devices_)), callback_(std::exchange(other.callback_, nullptr)) {}

MtwCallbackRegistration&
MtwCallbackRegistration::operator=(MtwCallbackRegistration&& other) noexcept {

    if (this != &other) {
        unregister();
        devices_ = std::move(other.devices_);
        callback_ = std::exchange(other.callback_, nullptr);
    }
    return *this;
}

void MtwCallbackRegistration::unregister() noexcept {
    if (!callback_) {
        return;
    }

    for (const XsDevicePtr& device : devices_) {
        if (device) {
            device->removeCallbackHandler(callback_);
        }
    }
    callback_ = nullptr;
    devices_.clear();
}

AwindaSystem::AwindaSystem() : control_(XsControl::construct()) {

    if (!control_) {
        throw std::runtime_error("Failed to construct the Xsens XsControl instance.");
    }
}

AwindaSystem::~AwindaSystem() {
    shutdown();
}

void AwindaSystem::configure(const int desiredUpdateRate, const int radioChannel) {

    std::cout << "Scanning USB ports for Awinda Station..." << std::endl;
    const XsPortInfoArray detectedDevices = XsScanner::scanPorts();

    bool foundMaster = false;
    for (const XsPortInfo& portInfo : detectedDevices) {
        if (portInfo.deviceId().isWirelessMaster()) {
            masterPort_ = portInfo;
            foundMaster = true;
            break;
        }
    }

    if (!foundMaster) {
        throw std::runtime_error("No Awinda Station detected. Check its power and USB connection.");
    }

    std::cout << "Awinda Station found on port: " << masterPort_.portName().toStdString() << " at "
              << XsBaud::rateToNumeric(masterPort_.baudrate()) << " baud" << std::endl;

    if (!control_->openPort(masterPort_.portName().toStdString(), masterPort_.baudrate())) {
        throw std::runtime_error("Failed to open the Awinda Station COM port.");
    }
    portOpen_ = true;

    wirelessMaster_ = control_->device(masterPort_.deviceId());
    if (!wirelessMaster_) {
        throw std::runtime_error("Failed to obtain the Xsens wireless-master device.");
    }

    std::cout << "Moving to Config mode..." << std::endl;
    if (!wirelessMaster_->gotoConfig()) {
        throw std::runtime_error("Failed to enter Xsens Config mode.");
    }

    wirelessMaster_->addCallbackHandler(&masterCallback_);
    masterCallbackRegistered_ = true;

    const XsIntArray supportedRates = wirelessMaster_->supportedUpdateRates();
    const int actualUpdateRate = findClosestUpdateRate(supportedRates, desiredUpdateRate);

    std::cout << "Supported Awinda update rates:";
    for (const int rate : supportedRates) {
        std::cout << ' ' << rate;
    }
    std::cout << " Hz" << std::endl;

    std::cout << "Setting update rate to " << actualUpdateRate << " Hz..." << std::endl;
    if (!wirelessMaster_->setUpdateRate(actualUpdateRate)) {
        throw std::runtime_error("Failed to set the Awinda update rate.");
    }

    std::cout << "Rebooting radio channel " << radioChannel << "..." << std::endl;
    if (wirelessMaster_->isRadioEnabled()) {
        wirelessMaster_->disableRadio();
    }
    if (!wirelessMaster_->enableRadio(radioChannel)) {
        throw std::runtime_error("Failed to enable the Awinda radio channel.");
    }
}

std::size_t AwindaSystem::connectedSensorCount() const {
    return masterCallback_.getWirelessMTWs().size();
}
std::set<std::string> AwindaSystem::connectedSensorIds() const {

    const std::set<XsDevice*> connectedDevices = masterCallback_.getWirelessMTWs();

    std::set<std::string> connectedIds;

    for (const XsDevice* device : connectedDevices) {

        if (!device) {
            continue;
        }

        connectedIds.insert(device->deviceId().toString().toStdString());
    }

    return connectedIds;
}

void AwindaSystem::refreshConnectedDevices() {
    mtwDevices_.clear();

    const XsDeviceIdArray deviceIds = control_->deviceIds();
    for (const XsDeviceId& id : deviceIds) {
        if (id.isMtw()) {
            const XsDevicePtr device = control_->device(id);
            if (device) {
                mtwDevices_.push_back(device);
            }
        }
    }

    if (mtwDevices_.empty()) {
        throw std::runtime_error("No connected MTw devices were available for configuration.");
    }
}

const std::vector<XsDevicePtr>& AwindaSystem::devices() const {
    return mtwDevices_;
}

void AwindaSystem::enterMeasurementMode() {
    if (!wirelessMaster_) {
        throw std::logic_error("AwindaSystem must be configured before measurement mode.");
    }

    std::cout << "Transitioning to Measurement Mode..." << std::endl;
    if (!wirelessMaster_->gotoMeasurement()) {
        throw std::runtime_error("Failed to enter Xsens Measurement mode.");
    }
}

MtwCallbackRegistration AwindaSystem::registerMtwCallback(XsCallback& callback) const {

    if (mtwDevices_.empty()) {
        throw std::logic_error("Cannot register a callback before resolving connected MTws.");
    }
    return MtwCallbackRegistration(mtwDevices_, callback);
}

void WirelessMasterCallback::onConnectivityChanged(XsDevice* device,
                                                   const XsConnectivityState state) {
    XsMutexLocker lock(m_mutex);

    const std::string deviceId = device->deviceId().toString().toStdString();
    switch (state) {
    case XCS_Wireless:
        std::cout << "\nEVENT: MTW Connected -> " << deviceId << std::endl;
        m_connectedMTWs.insert(device);
        break;
    case XCS_Disconnected:
        std::cout << "\nEVENT: MTW Disconnected -> " << deviceId << std::endl;
        m_connectedMTWs.erase(device);
        break;
    case XCS_Rejected:
        std::cout << "\nEVENT: MTW Rejected -> " << deviceId << std::endl;
        m_connectedMTWs.erase(device);
        break;
    case XCS_PluggedIn:
        std::cout << "\nEVENT: MTW PluggedIn -> " << deviceId << std::endl;
        m_connectedMTWs.erase(device);
        break;
    case XCS_File:
        std::cout << "\nEVENT: MTW File -> " << deviceId << std::endl;
        m_connectedMTWs.erase(device);
        break;
    case XCS_Unknown:
        std::cout << "\nEVENT: MTW Unknown -> " << deviceId << std::endl;
        m_connectedMTWs.erase(device);
        break;
    default:
        std::cout << "\nEVENT: MTW Error -> " << deviceId << std::endl;
        m_connectedMTWs.erase(device);
        break;
    }
}

std::set<XsDevice*> WirelessMasterCallback::getWirelessMTWs() const {
    XsMutexLocker lock(m_mutex);
    return m_connectedMTWs;
}

void AwindaSystem::shutdown() noexcept {
    mtwDevices_.clear();

    if (wirelessMaster_) {
        if (masterCallbackRegistered_) {
            wirelessMaster_->removeCallbackHandler(&masterCallback_);
            masterCallbackRegistered_ = false;
        }

        if (wirelessMaster_->isRadioEnabled()) {
            wirelessMaster_->disableRadio();
        }
        wirelessMaster_ = nullptr;
    }

    if (control_) {
        if (portOpen_) {
            control_->closePort(masterPort_);
            portOpen_ = false;
        }
        control_->destruct();
        control_ = nullptr;
    }
}
