
#include "gps.h"

#include <gps.h>
#include <unistd.h>

#include <cmath>
#include <ctime>
#include <chrono>

#define POSITION_LIB_NAMESPACE_BEGIN
#define POSITION_LIB_NAMESPACE_END
#define POSITION_LIB_DETAIL_NAMESPACE_BEGIN
#define POSITION_LIB_DETAIL_NAMESPACE_REFERENCE
#define POSITION_LIB_DETAIL_NAMESPACE_END
#include "external/position.hpp"

using namespace std;

struct gpsd_client::gpsd_client_impl
{
    gps_data_t gps_data;
};

gpsd_client::gpsd_client()
{
    impl = make_unique<gpsd_client_impl>();
}

gpsd_client::~gpsd_client() = default;

bool gpsd_client::open(const string& hostname, int port)
{
    if (gps_open(hostname.c_str(), to_string(port).c_str(), &impl.get()->gps_data) != 0)
    {
        return false;
    }
    gps_stream(&impl.get()->gps_data, WATCH_ENABLE | WATCH_JSON, nullptr);
    return true;
}

void gpsd_client::close()
{
    gps_stream(&impl.get()->gps_data, WATCH_DISABLE, NULL);
    gps_close(&impl.get()->gps_data);
}

bool gpsd_client::try_get_gps_info(gnss_info& info, gnss_include_info include_info)
{
    const char* mode_str[] =
    {
        "n/a",
        "None",
        "2D", 
        "3D"
    };

    bool position_set = false;
    bool satellites_set = false;
    bool time_set = false;

    auto start = std::chrono::high_resolution_clock::now();

    while (true)
    {
        if (!gps_waiting(&impl.get()->gps_data, 100000))
        {
            continue;
        }

        if (gps_read(&impl.get()->gps_data, NULL, 0) == -1)
        {
            return false;
        }

        if ((MODE_SET & impl.get()->gps_data.set) != MODE_SET)
        {
            continue;
        }

        if (impl.get()->gps_data.fix.mode < 0 || impl.get()->gps_data.fix.mode >= sizeof(mode_str))
        {
            impl.get()->gps_data.fix.mode = 0;
        }

        if (TIME_SET == (TIME_SET & impl.get()->gps_data.set))
        {
            std::chrono::system_clock::time_point tp
            {
                std::chrono::seconds(impl.get()->gps_data.fix.time.tv_sec) + std::chrono::nanoseconds(impl.get()->gps_data.fix.time.tv_nsec)
            };

            std::time_t time_t = std::chrono::system_clock::to_time_t(tp);
            std::tm* timeinfo = std::gmtime(&time_t);

            if (timeinfo == nullptr)
            {
                return false;
            }

            info.time_utc.year = timeinfo->tm_year + 1900;
            info.time_utc.month = timeinfo->tm_mon + 1;
            info.time_utc.day = timeinfo->tm_mday;     
            info.time_utc.hour = timeinfo->tm_hour;    
            info.time_utc.minute = timeinfo->tm_min;   
            info.time_utc.second = timeinfo->tm_sec;   

            std::tm* timeinfo_local = std::localtime(&time_t);

            if (timeinfo_local == nullptr)
            {
                return false;
            }

            info.time.year = timeinfo_local->tm_year + 1900;
            info.time.month = timeinfo_local->tm_mon + 1;
            info.time.day = timeinfo_local->tm_mday;     
            info.time.hour = timeinfo_local->tm_hour;    
            info.time.minute = timeinfo_local->tm_min;   
            info.time.second = timeinfo_local->tm_sec;  

            time_set = true;
        }

        if (SATELLITE_SET == (SATELLITE_SET & impl.get()->gps_data.set))
        {
            info.satellites = impl.get()->gps_data.satellites_used;
            satellites_set = true;
        }       

        if (isfinite(impl.get()->gps_data.fix.latitude) && isfinite(impl.get()->gps_data.fix.longitude))
        {
            info.lat = impl.get()->gps_data.fix.latitude;
            info.lon = impl.get()->gps_data.fix.longitude;
            info.speed = impl.get()->gps_data.fix.speed;
            info.alt = impl.get()->gps_data.fix.altitude;
            info.track = impl.get()->gps_data.fix.track;

            auto sattelites = impl.get()->gps_data.satellites_visible; 
            auto lat_error = impl.get()->gps_data.fix.epy;
            auto lon_error = impl.get()->gps_data.fix.epx;            
            auto speed_error = impl.get()->gps_data.fix.eps;

            switch (impl.get()->gps_data.fix.mode)
            {
                case 0:
                case 1:
                    info.mode = fix_mode::none;
                    break;
                case 2:
                    info.mode = fix_mode::d2;
                    break;
                case 3:
                    info.mode = fix_mode::d3;
                    break;
                default:
                    info.mode = fix_mode::none;
                    break;
            }

            position_set = true;
        }
       
        if ((!enum_gnss_include_info_has_flag(include_info, gnss_include_info::position) || position_set) &&
            (!enum_gnss_include_info_has_flag(include_info, gnss_include_info::time) || time_set) &&
            (!enum_gnss_include_info_has_flag(include_info, gnss_include_info::satellites) || satellites_set))
        {
            auto end = std::chrono::high_resolution_clock::now();
            info.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            break;
        }
    }

    return true;
}

bool gpsd_client::try_get_gps_position_and_time(double& lat, double& lon, struct date_time& time_utc)
{
    gnss_info info;
    if (try_get_gps_info(info, (gnss_include_info::position | gnss_include_info::time)))
    {
        lat = info.lat;
        lon = info.lon;
        time_utc = info.time_utc;
        return true;
    }
    return false;
}

std::string to_json(const gnss_info& info)
{
    std::string str;

    str += "{\n";
    
    // DD    
    position_dd dd(info.lat, info.lon);
    str += "    \"position_dd\": {\n";
    str += "        \"lat\": \"" + std::to_string(dd.lat) + "\",\n";
    str += "        \"lon\": \"" + std::to_string(dd.lon) + "\"\n";
    str += "    },\n";

    // DDM
    position_ddm ddm = dd;
    str += "    \"position_ddm\": {\n";
    str += "        \"lat\": \"" + std::string(1, ddm.lat) + "\",\n";
    str += "        \"lat_d\": \"" + std::to_string(ddm.lat_d) + "\",\n";
    str += "        \"lat_m\": \"" + std::to_string(ddm.lat_m) + "\",\n";
    str += "        \"lon\": \"" + std::string(1, ddm.lon) + "\",\n";
    str += "        \"lon_d\": \"" + std::to_string(ddm.lon_d) + "\",\n";
    str += "        \"lon_m\": \"" + std::to_string(ddm.lon_m) + "\"\n";
    str += "    },\n";

    // DMS
    position_dms dms = dd;
    str += "    \"position_dms\": {\n";
    str += "        \"lat\": \"" + std::string(1, dms.lat) + "\",\n";
    str += "        \"lat_d\": \"" + std::to_string(dms.lat_d) + "\",\n";
    str += "        \"lat_m\": \"" + std::to_string(dms.lat_m) + "\",\n";
    str += "        \"lat_s\": \"" + std::to_string(dms.lat_s) + "\",\n";
    str += "        \"lon\": \"" + std::string(1, dms.lon) + "\",\n";
    str += "        \"lon_d\": \"" + std::to_string(dms.lon_d) + "\",\n";
    str += "        \"lon_m\": \"" + std::to_string(dms.lon_m) + "\",\n";
    str += "        \"lon_s\": \"" + std::to_string(dms.lon_s) + "\"\n";    
    str += "    },\n";

    // DDM in ddmm.mmN/dddmm.mmE notation used by APRX
    position_display_string pos_display = format(ddm, position_ddm_short_format);
    str += "    \"position_ddm_short\": {\n";
    str += "        \"lat\": \"" + pos_display.lat + "\",\n";
    str += "        \"lon\": \"" + pos_display.lon + "\"\n";
    str += "    },\n";

    str += "    \"altitude\": \"" + std::to_string(info.alt) +  "\",\n";
    str += "    \"speed\": \"" + std::to_string(info.speed) +  "\",\n";
    str += "    \"track\": \"" + std::to_string(info.track) +  "\",\n";
    str += "    \"satellites_used\": \"" + std::to_string(info.satellites) +  "\",\n";

    date_time t_utc = info.time_utc;
    str += "    \"utc_time\": {\n";
    str += "        \"year\": \"" + std::to_string(t_utc.year) + "\",\n";
    str += "        \"month\": \"" + ((t_utc.month < 10 && t_utc.month > 0) ? ("0" + std::to_string(t_utc.month)) : std::to_string(t_utc.month)) + "\",\n";
    str += "        \"day\": \"" + ((t_utc.day < 10 && t_utc.day > 0) ? ("0" + std::to_string(t_utc.day)) : std::to_string(t_utc.day)) + "\",\n";
    str += "        \"hour\": \"" + ((t_utc.hour < 10 && t_utc.hour > 0) ? ("0" + std::to_string(t_utc.hour)) : std::to_string(t_utc.hour)) + "\",\n";
    str += "        \"min\": \"" + ((t_utc.minute < 10 && t_utc.minute > 0) ? ("0" + std::to_string(t_utc.minute)) : std::to_string(t_utc.minute)) + "\",\n";
    str += "        \"sec\": \"" + ((t_utc.second < 10 && t_utc.second > 0) ? ("0" + std::to_string(t_utc.second)) : std::to_string(t_utc.second)) + "\"\n";
    str += "    },\n";

    date_time t = info.time;
    str += "    \"time\": {\n";
    str += "        \"year\": \"" + std::to_string(t.year) + "\",\n";
    str += "        \"month\": \"" + ((t.month < 10 && t.month > 0) ? ("0" + std::to_string(t.month)) : std::to_string(t.month)) + "\",\n";
    str += "        \"day\": \"" + ((t.day < 10 && t.day > 0) ? ("0" + std::to_string(t.day)) : std::to_string(t.day)) + "\",\n";
    str += "        \"hour\": \"" + ((t.hour < 10 && t.hour > 0) ? ("0" + std::to_string(t.hour)) : std::to_string(t.hour)) + "\",\n";
    str += "        \"min\": \"" + ((t.minute < 10 && t.minute > 0) ? ("0" + std::to_string(t.minute)) : std::to_string(t.minute)) + "\",\n";
    str += "        \"sec\": \"" + ((t.second < 10 && t.second > 0) ? ("0" + std::to_string(t.second)) : std::to_string(t.second)) + "\"\n";
    str += "    }\n";
    
    str += "}";

    return str;
}