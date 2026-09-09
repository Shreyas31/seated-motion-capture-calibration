#pragma once

#include "xsmutex.h"

#include <set>
#include <xsensdeviceapi.h>

/**
 * Monitors MTw sensor connectivity changes reported by the Awinda master.
 * @class WirelessMasterCallback.
 */
class WirelessMasterCallback : public XsCallback {
  private:
    mutable XsMutex m_mutex;
    std::set<XsDevice*> m_connectedMTWs;

  protected:
    /**
     * Updates the connected-device set when XDA reports a connectivity transition.
     * @param dev Device whose connectivity state changed.
     * @param newState New Xsens connectivity state.
     */
    void onConnectivityChanged(XsDevice* dev, XsConnectivityState newState) override;

  public:
    /**
     * Returns a thread-safe snapshot of currently connected MTw devices.
     * @return Set of non-owning Xsens device pointers.
     */
    std::set<XsDevice*> getWirelessMTWs() const;
};
