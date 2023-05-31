
#include "gps.h"

#include <gps.h>
#include <unistd.h>
#include <cmath>

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

bool gpsd_client::try_get_gps_position(double& lat, double& lon)
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

        if (isfinite(impl.get()->gps_data.fix.latitude) && isfinite(impl.get()->gps_data.fix.longitude))
        {
            lat = impl.get()->gps_data.fix.latitude;
            lon = impl.get()->gps_data.fix.longitude;
            break;
        }
        else
        {
        }
    }

    return true;
}
