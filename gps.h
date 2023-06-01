#pragma once

#include <string>
#include <memory>

struct date_time
{
    int year = -1;
    int month = -1;
    int day = -1;     
    int hour = -1;    
    int minute = -1;   
    int second = -1; 
};

class gpsd_client
{
public:
    gpsd_client();
    ~gpsd_client();
    gpsd_client(const gpsd_client&) = default;
    gpsd_client& operator=(const gpsd_client&) = default;
    bool open(const std::string& hostname = "localhost", int port = 2947);
    void close();
    bool try_get_gps_position_and_time(double& lat, double& lon, struct date_time& time);
private:
    struct gpsd_client_impl;
    std::unique_ptr<gpsd_client_impl> impl;
};
