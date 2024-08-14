#pragma once

#include <string>
#include <memory>
#include <limits>

struct date_time
{
    int year = -1;
    int month = -1;
    int day = -1;     
    int hour = -1;    
    int minute = -1;   
    int second = -1; 
};

enum class fix_mode
{
    none,
    d2,
    d3
};

struct gnss_info
{
    double lat = std::numeric_limits<double>::quiet_NaN();
    double lon = std::numeric_limits<double>::quiet_NaN();
    double speed = std::numeric_limits<double>::quiet_NaN();
    double alt = std::numeric_limits<double>::quiet_NaN();
    double track = std::numeric_limits<double>::quiet_NaN();
    date_time time_utc;
    date_time time;
    int age = -1;
    fix_mode mode = fix_mode::none;
    int satellites = 0;
    int duration = 0;
};

std::string to_json(const gnss_info& info);

enum class gnss_include_info : int
{
    none = 0,
    position = 1,
    satellites = 2,
    time = 4,
    all = position | satellites | time
};

inline gnss_include_info operator|(const gnss_include_info& l, const gnss_include_info& r)
{
    return (gnss_include_info)((int)l | (int)r);
}

inline gnss_include_info operator&(const gnss_include_info& l, const gnss_include_info& r)
{
    return (gnss_include_info)((int)l & (int)r);
}

inline bool enum_gnss_include_info_has_flag(const gnss_include_info& include_info, const gnss_include_info& flag)
{
    return (include_info & flag) != (gnss_include_info)0;
}

class gpsd_client
{
public:
    gpsd_client();
    ~gpsd_client();
    gpsd_client(const gpsd_client&) = default;
    gpsd_client& operator=(const gpsd_client&) = default;
    bool open(const std::string& hostname = "localhost", int port = 2947);
    void close();
    bool try_get_gps_position_and_time(double& lat, double& lon, struct date_time& time_utc);
    bool try_get_gps_info(gnss_info& info, gnss_include_info include_info);
private:
    struct gpsd_client_impl;
    std::unique_ptr<gpsd_client_impl> impl;
};
