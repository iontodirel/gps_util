
#include "gps.h"

#include <gps.h>
#include <unistd.h>
#include <cmath>

#include <ctime>
#include <chrono>

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

bool gpsd_client::try_get_gps_position_and_time(double& lat, double& lon, struct date_time& t)
{
    const char* mode_str[] =
    {
        "n/a",
        "None",
        "2D",
        "3D"
    };

    while (true)
    {
        gps_waiting(&impl.get()->gps_data, 5000000);

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

            t.year = timeinfo->tm_year + 1900;
            t.month = timeinfo->tm_mon + 1;
            t.day = timeinfo->tm_mday;     
            t.hour = timeinfo->tm_hour;    
            t.minute = timeinfo->tm_min;   
            t.second = timeinfo->tm_sec;   
        }

        if (isfinite(impl.get()->gps_data.fix.latitude) && isfinite(impl.get()->gps_data.fix.longitude))
        {
            lat = impl.get()->gps_data.fix.latitude;
            lon = impl.get()->gps_data.fix.longitude;
            break;
        }
    }

    return true;
}
