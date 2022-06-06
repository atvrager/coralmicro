#ifndef LIBS_BASE_WIFI_H_
#define LIBS_BASE_WIFI_H_

#include <optional>
#include <vector>

#include "libs/base/utils.h"

extern "C" {
#include "libs/nxp/rt1176-sdk/rtos/freertos/libraries/abstractions/wifi/include/iot_wifi.h"
}

namespace coral::micro {

inline constexpr int kDefaultRetryCount{5};

// Represents the Wi-Fi antenna.
enum class WifiAntenna {
    // Internal built in Wi-Fi antenna.
    kInternal = 0,
    // External custom Wi-Fi antenna.
    kExternal = 1,
};

// Turns on the Wi-Fi module.
//
// @return True if successfully turned on; false otherwise.
bool TurnOnWiFi();

// Turns off the Wi-Fi module.
//
// @return True if successfully turned off; false otherwise.
bool TurnOffWiFi();

// Checks if the board is connected to a Wi-Fi network.
//
// @return True if it is connected; false otherwise.
bool WifiIsConnected();

// Connects to a Wi-Fi network.
//
// @params network_params A pointer to a `WIFINetworkParams_t` that contains
// information of the ssid such as name, password, and security type. See:
// https://aws.github.io/amazon-freertos/202107.00/html1/struct_w_i_f_i_network_params__t.html
// @params retry_count The max number of connection attempts. Default is 5.
// @return True if successfully connected to Wi-Fi; false otherwise.
bool ConnectWifi(const WIFINetworkParams_t* network_params,
                 int retry_count = kDefaultRetryCount);

// Connects to a Wi-Fi network with the given network name and password.
//
// @params ssid The network name.
// @params psk The password for the ssid.
// @params retry_count The max number of connection attempts. Default is 5.
// @return True if successfully connected to Wi-Fi; false otherwise.
bool ConnectWiFi(const char* ssid, const char* psk,
                 int retry_count = kDefaultRetryCount);

// Connects to the Wi-Fi network that's saved on the device.
//
// Internally, this API reads the stored ssid and password using
// `utils::GetWifiSSID()` and `utils::GetWifiPSK()`, which could both be set either
// during flash with the `--wifi_ssid` and `--wifi_psk` flags or with a direct
// call to `utils::SetWifiSSID()` and `utils::SetWifiPSK()`.
//
// @params retry_count The max number of connection attempts. Default is 5.
// @return True if successfully connected to Wi-Fi; false otherwise.
bool ConnectWiFi(int retry_count = kDefaultRetryCount);

// Disconnects from the Wi-Fi network.
//
// @params retry_count The max number of disconnect attempts. Default is 5.
// @return True if Wi-Fi is successfully disconnected; false otherwise.
bool DisconnectWifi(int retry_count = kDefaultRetryCount);

// Scans for Wi-Fi networks.
//
// @params max_results The max number of networks to look for.
// This function may return fewer than this number but never more.
// Default is 255 which is the maximum for uint8_t.
// @return A vector of `WIFIScanResult_t` which contains info like name, security
// type, etc. See:
// https://aws.github.io/amazon-freertos/202107.00/html1/struct_w_i_f_i_scan_result__t.html
std::vector<WIFIScanResult_t> ScanWifi(
    uint8_t max_results = std::numeric_limits<uint8_t>::max());

// Gets the device's Wi-Fi IP address.
//
// @return A string representing the IPv4 IP address or `std::nullopt` on
// failure.
std::optional<std::string> GetWifiIp();

// Sets which Wi-Fi antenna type to use (internal or external).
//
// @params antenna The type of antenna to use.
// @return True if the Wi-Fi antenna was enabled successfully; false otherwise.
bool SetWifiAntenna(WifiAntenna antenna);

}  // namespace coral::micro

#endif  // LIBS_BASE_WIFI_H_
