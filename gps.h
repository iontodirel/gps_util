#pragma once

#include <string>
#include <memory>

class gpsd_client
{
public:
    gpsd_client();
    ~gpsd_client();
    gpsd_client(const gpsd_client&) = default;
    gpsd_client& operator=(const gpsd_client&) = default;
    bool open(const std::string& hostname = "localhost", int port = 2947);
    void close();
    bool try_get_gps_position(double& lat, double& lon);
private:
    struct gpsd_client_impl;
    std::unique_ptr<gpsd_client_impl> impl;
};
