#include "gps.h"

#include <cxxopts.hpp>
#include <fmt/format.h>

#include <iostream>
#include <fstream>

#define POSITION_LIB_NAMESPACE_BEGIN
#define POSITION_LIB_NAMESPACE_END
#include "external/position.hpp"

using namespace std;

struct args;

enum class position_print_format
{
    dd,
    dms,
    ddm,
    ddm_short
};

struct args
{
    std::string host_name = "localhost";
    int port = 8888;
    std::string output_file = "";
    position_print_format format = position_print_format::dd;
    std::string command_line_error;
    bool command_line_has_errors = false;
    bool help = false;
    bool no_stdout = false;
};

position_print_format parse_position_format(const std::string& pos_str)
{
    if (pos_str == "dd")
        return position_print_format::dd;
    else if (pos_str == "dms")
        return position_print_format::dms;
    else if (pos_str == "ddm")
        return position_print_format::ddm;
    else if (pos_str == "ddm_short" || pos_str == "aprx")
        return position_print_format::ddm_short;
    return position_print_format::dd;
}

bool try_get_gps_position_and_time(const args& args, position_dd& pos, struct date_time& t);
bool try_parse_command_line(int argc, char* argv[], args& args);
void print_usage();
std::string coordinate_to_aprx_format(int d_d, double d_m, char d);
void print_position(position_format format, position_dd& dd);
std::string to_json(position_dd& dd, date_time& t);
int write_position(const std::string& filename, position_dd& dd, date_time& t);

bool try_parse_command_line(int argc, char* argv[], args& args)
{
    cxxopts::Options options("", "");
    
    options
        .add_options()
        ("o,output", "", cxxopts::value<std::string>())
        ("f,format", "", cxxopts::value<std::string>()->default_value("dd"))
        ("p,port", "", cxxopts::value<int>())
        ("h,host-name", "", cxxopts::value<std::string>())
        ("help", "")
        ("no-stdout", "");

    cxxopts::ParseResult result;
    
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::incorrect_argument_type& e)
    {
        args.command_line_error = fmt::format("Error parsing command line: {}\n\n", e.what());
        args.command_line_has_errors = true;
        return false;
    }
    catch (const std::exception& e)
    {
        args.command_line_error = "Error parsing command line.";
        args.command_line_has_errors = true;
        return false;
    }

    if (result.count("output") > 0)
        args.output_file = result["output"].as<std::string>();
    if (result.count("format") > 0)
        args.format = parse_position_format(result["format"].as<std::string>());
    if (result.count("port") > 0)
        args.port = result["port"].as<int>();
    if (result.count("host-name") > 0)
        args.host_name = result["host-name"].as<std::string>();
    if (result.count("help") > 0)
        args.help = true;
    if (result.count("no-stdout") > 0)
        args.no_stdout = true;

    return true;
}

void print_usage()
{
    std::string usage =
        "gps_util - simple gpsd client\n"
        "(C) 2023 Ion Todirel\n"
        "\n"
        "Usage:\n"
        "    gps_util [OPTION]... \n"
        "\n"
        "Options:\n"     
        "    -h, --host-name <host>  specify the hostname where gpsd runs on\n"
        "    -p, --port <port>       specify the port to connect to gpsd, typically 2947\n"
        "    -f, --format <format>   coordinates print format:\n"
        "                                dd\n"
        "                                ddm\n"
        "                                dms\n"
        "                                ddm_short\n"
        "    -o, --output <file>     file name to write JSON GPS data to\n"
        "    --help                  print usage\n"
        "    --no-stdout             no stdout\n"
        "\n"
        "Returns:\n"
        "    0 - success\n"
        "    1 - failure\n"
        "\n"
        "Example:\n"
        "    gps_util -h localhost -p 8888 -f dms -o file.json -h\n"
        "\n"
        "\n";
    printf("%s", usage.c_str());
}

std::string coordinate_to_aprx_format(int d_d, double d_m, char d)
{
    std::string res;

    double d_m_i, d_m_f;    
    d_m_f = std::modf(d_m, &d_m_i);

    res.append(std::to_string(d_d));
    res.append(std::to_string((int)d_m_i));
    res.append(".");

    double d_m_f_100 = (format_number(d_m_f, 2) * 100);

    if (d_m_f_100 < 10)
    {
        res.append("0");
    }

    res.append(std::to_string((int)d_m_f_100));
    res.append(std::string(1, d));

    return res;
}

void print_position(position_print_format print_fmt, position_dd& dd)
{
    position_display_string pos_display;

    if (print_fmt == position_print_format::dd)
    {
        pos_display = format(dd, position_dd_format);
    }
    else if (print_fmt == position_print_format::ddm)
    {
        pos_display = format(position_ddm(dd), position_ddm_format);
    }
    else if (print_fmt == position_print_format::dms)
    {
        pos_display = format(position_dms(dd), position_dms_format);
    }
    else if (print_fmt == position_print_format::ddm_short)
    {
        position_ddm ddm = dd;
        std::string lat = coordinate_to_aprx_format(ddm.lat_d, ddm.lat_m, ddm.lat);
        std::string lon = coordinate_to_aprx_format(ddm.lon_d, ddm.lon_m, ddm.lon);
        printf("%s, %s\n", lat.c_str(), lon.c_str());
        return;
    }  

    printf("%s, %s\n", pos_display.lat.c_str(), pos_display.lon.c_str());
}

int write_position(const std::string& filename, position_dd& dd, date_time& t)
{
    std::string json = to_json(dd, t);

    std::ofstream outputFile(filename);
    if (!outputFile)
    {
        return 1;
    }

    outputFile << json;
    outputFile.close();

    return 0;
}

std::string to_json(position_dd& dd, date_time& t)
{
    std::string str;
    str += "{\n";
    
    // DD    
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
    str += "    \"position_ddm_short\": {\n";
    str += "        \"lat\": \"" + coordinate_to_aprx_format(ddm.lat_d, ddm.lat_m, ddm.lat) + "\",\n";
    str += "        \"lon\": \"" + coordinate_to_aprx_format(ddm.lon_d, ddm.lon_m, ddm.lon) + "\"\n";
    str += "    },\n";

    str += "    \"utc_time\": {\n";
    str += "        \"year\": \"" + std::to_string(t.year) + "\",\n";
    str += "        \"month\": \"" + ((t.month < 10) ? ("0" + std::to_string(t.month)) : std::to_string(t.month)) + "\",\n";
    str += "        \"day\": \"" + ((t.day < 10) ? ("0" + std::to_string(t.day)) : std::to_string(t.day)) + "\",\n";
    str += "        \"hour\": \"" + ((t.hour < 10) ? ("0" + std::to_string(t.hour)) : std::to_string(t.hour)) + "\",\n";
    str += "        \"min\": \"" + ((t.minute < 10) ? ("0" + std::to_string(t.minute)) : std::to_string(t.minute)) + "\",\n";
    str += "        \"sec\": \"" + ((t.second < 10) ? ("0" + std::to_string(t.second)) : std::to_string(t.second)) + "\"\n";
    str += "    }\n";

    str += "}";
    return str;
}

int main(int argc, char* argv[])
{
    args args;

    try_parse_command_line(argc, argv, args);

    if (args.help && !args.no_stdout)
    {
        print_usage();
        return 1;
    }    

    if (args.command_line_has_errors && !args.no_stdout)
    {
        printf("%s\n\n", args.command_line_error.c_str());
        print_usage();
        return 1;
    }

    position_dd pos;
    date_time t;

    if (try_get_gps_position_and_time(args, pos, t))
    {
        if (!args.no_stdout)
        {
            print_position(args.format, pos);
        }
        if (!args.output_file.empty())
        {
            return write_position(args.output_file, pos, t);
        }
        return 0;
    }

    return 1;
}

bool try_get_gps_position_and_time(const args& args, position_dd& pos, date_time& t)
{
    gpsd_client s;
    if (s.open(args.host_name, args.port))
    {
        s.try_get_gps_position_and_time(pos.lat, pos.lon, t);
        s.close();
        return true;
    }
    return false;
}