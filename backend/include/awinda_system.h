#pragma once

#include "wireless_master_callback.h"

#include <cstddef>
#include <set>
#include <string>
#include <vector>
#include <xsensdeviceapi.h>

/**
 * RAII registration of one callback with all connected MTw devices.
 * @class MtwCallbackRegistration.
 */
class MtwCallbackRegistration {
  public:
    /** Creates an empty registration that owns no callback attachments. */
    MtwCallbackRegistration() = default;

    /** Unregisters the callback from every device still owned by this object. */
    ~MtwCallbackRegistration();

    MtwCallbackRegistration(const MtwCallbackRegistration&) = delete;
    MtwCallbackRegistration& operator=(const MtwCallbackRegistration&) = delete;

    /**
     * Transfers callback-registration ownership from another object.
     * @param other Registration whose device attachments are transferred.
     */
    MtwCallbackRegistration(MtwCallbackRegistration&& other) noexcept;

    /**
     * Releases current attachments and transfers ownership from another object.
     * @param other Registration whose device attachments are transferred.
     * @return Reference to this registration.
     */
    MtwCallbackRegistration& operator=(MtwCallbackRegistration&& other) noexcept;

  private:
    friend class AwindaSystem;

    /**
     * Registers one callback with every supplied device.
     * @param devices Connected MTw devices that receive the callback.
     * @param callback Callback whose lifetime must exceed this registration.
     */
    MtwCallbackRegistration(const std::vector<XsDevicePtr>& devices, XsCallback& callback);

    /** Removes the owned callback registration from all devices. */
    void unregister() noexcept;

    std::vector<XsDevicePtr> devices_;
    XsCallback* callback_ = nullptr;
};

/**
 * Owns the complete Xsens Awinda hardware lifecycle.
 * @class AwindaSystem.
 */
class AwindaSystem {
  public:
    /**
     * Acquires an Xsens control object.
     * @throws std::runtime_error if the Xsens control object cannot be created.
     */
    AwindaSystem();

    /** Disables radio communication, closes the port, and releases Xsens resources. */
    ~AwindaSystem();

    AwindaSystem(const AwindaSystem&) = delete;
    AwindaSystem& operator=(const AwindaSystem&) = delete;
    AwindaSystem(AwindaSystem&&) = delete;
    AwindaSystem& operator=(AwindaSystem&&) = delete;

    /**
     * Discovers and configures an Awinda master in radio configuration mode.
     * @param desiredUpdateRate Requested packet update rate in hertz.
     * @param radioChannel Awinda radio channel number.
     * @throws std::runtime_error if discovery, port setup, rate selection, or radio setup fails.
     */
    void configure(int desiredUpdateRate, int radioChannel);

    /**
     * Returns the number of MTw sensors currently reported as connected.
     * @return Connected wearable-sensor count.
     */
    std::size_t connectedSensorCount() const;

    /**
     * Returns physical IDs of MTw sensors currently reported as connected.
     * @return Snapshot of connected device ID strings.
     */
    std::set<std::string> connectedSensorIds() const;

    /**
     * Resolves the currently connected MTw IDs to device objects.
     * @throws std::runtime_error if a reported sensor cannot be resolved to an Xsens device.
     */
    void refreshConnectedDevices();

    /**
     * Returns device objects resolved by the latest refresh.
     * @return Reference to the connected MTw device list.
     */
    const std::vector<XsDevicePtr>& devices() const;

    /**
     * Switches the configured wireless master into live measurement mode.
     * @throws std::runtime_error if the master rejects the mode transition.
     */
    void enterMeasurementMode();

    /**
     * Registers one live-data callback with every refreshed MTw device.
     * @param callback Callback whose lifetime must exceed the returned registration.
     * @return RAII object that unregisters the callback on destruction.
     */
    MtwCallbackRegistration registerMtwCallback(XsCallback& callback) const;

  private:
    /** Releases every acquired hardware resource without propagating exceptions. */
    void shutdown() noexcept;

    XsControl* control_ = nullptr;
    XsPortInfo masterPort_;
    XsDevicePtr wirelessMaster_;
    std::vector<XsDevicePtr> mtwDevices_;
    WirelessMasterCallback masterCallback_;
    bool portOpen_ = false;
    bool masterCallbackRegistered_ = false;
};
