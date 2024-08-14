#include "gps.h"

#include <cxxopts.hpp>
#include <fmt/format.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <algorithm>

#define POSITION_LIB_NAMESPACE_BEGIN
#define POSITION_LIB_NAMESPACE_END
#define POSITION_LIB_DETAIL_NAMESPACE_BEGIN
#define POSITION_LIB_DETAIL_NAMESPACE_REFERENCE
#define POSITION_LIB_DETAIL_NAMESPACE_END
#include "external/position.hpp"

using namespace std;

// **************************************************************** //
//                                                                  //
//                                                                  //
// DECLARATIONS                                                     //
//                                                                  //
//                                                                  //
// **************************************************************** //

struct args;

enum class position_print_format
{
    dd,
    dms,
    ddm,
    ddm_short,
    aprs
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
    bool no_gps = false;
    double lat = -1;
    double lon = -1;
    std::string aprs_comment;
    std::string aprs_symbol;
    std::string aprs_symbol_table;
};

bool try_parse_command_line(int argc, char* argv[], args& args);
void print_usage();
std::string to_lower(const std::string& s);
bool try_parse_bool(const std::string& s, bool& b);

position_print_format parse_position_format(const std::string& pos_str);

void print_position(position_format format, const gnss_info& gnss_info);
int write_position(const std::string& filename, const gnss_info& info);
std::string encode_aprs_position_packet_no_time(const std::string& symbol, const std::string& symbol_table, const std::string& comment, double lat, double lon);
std::string encode_aprs_position_packet_no_time(const args& args, double lat, double lon);
std::string encode_aprs_position_packet_no_time(const args& args);
std::string encode_aprs_position_packet(const std::string& symbol, const std::string& symbol_table, const std::string& comment, const gnss_info& gnss_info);
std::string encode_aprs_position_packet(const args& args, const gnss_info& gnss_info);
void print_aprs_position_packet(const args& args, const gnss_info& gnss_info);
std::string format_two_digits_string(int number);

bool try_get_gps_info(const args& args, gnss_info& info);

int main(int argc, char* argv[]);

// **************************************************************** //
//                                                                  //
//                                                                  //
//                                                                  //
//                                                                  //
// IMPLEMENTATION                                                   //
//                                                                  //
//                                                                  //
//                                                                  //
//                                                                  //
// **************************************************************** //

bool try_parse_command_line(int argc, char* argv[], args& args)
{
    cxxopts::Options options("", "");
    
    options
        .add_options()
        ("o,output", "", cxxopts::value<std::string>())
        ("f,format", "", cxxopts::value<std::string>()->default_value("dd"))
        ("p,port", "", cxxopts::value<int>())
        ("h,host-name", "", cxxopts::value<std::string>())
        ("aprs-comment", "", cxxopts::value<std::string>())
        ("aprs-symbol", "", cxxopts::value<std::string>())
        ("aprs-symbol-table-id", "", cxxopts::value<std::string>())        
        ("lat", "", cxxopts::value<std::string>())
        ("lon", "", cxxopts::value<std::string>())
        ("use-gps", "", cxxopts::value<std::string>())
        ("no-gps", "")
        ("help", "")
        ("no-stdout", "");

    cxxopts::ParseResult result;
    
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const std::exception& e)
    {
        args.command_line_error = fmt::format("Error parsing command line: {}\n\n", e.what());
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
    if (result.count("use-gps") > 0)
    {
        bool use_gps = false;
        try_parse_bool(result["use-gps"].as<std::string>(), use_gps);
        args.no_gps = !use_gps;
    }
    if (result.count("no-gps") > 0)
        args.no_gps = true;
    if (result.count("lat") > 0)
        args.lat = atof(result["lat"].as<std::string>().c_str());
    if (result.count("lon") > 0)
        args.lon = atof(result["lon"].as<std::string>().c_str());
    if (result.count("aprs-comment") > 0)
        args.aprs_comment = result["aprs-comment"].as<std::string>();
    if (result.count("aprs-symbol") > 0)
        args.aprs_symbol = result["aprs-symbol"].as<std::string>();
    if (result.count("aprs-symbol-table-id") > 0)
        args.aprs_symbol_table = result["aprs-symbol-table-id"].as<std::string>();

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
        "    -h, --host-name <host>       specify the hostname where gpsd runs on\n"
        "    -p, --port <port>            specify the port to connect to gpsd, typically 2947\n"
        "    -f, --format <format>        coordinates print format:\n"
        "                                     dd\n"
        "                                     ddm\n"
        "                                     dms\n"
        "                                     ddm_short\n"
        "                                     aprx\n"
        "                                     aprs\n"
        "    --aprs-comment <comment>     APRS comment\n"
        "    --aprs-symbol <symbol>       APRS symbol\n"
        "    --aprs-symbol-table-id <id>  APRS symbol table\n"
        "    -o, --output <file>          file name to write JSON GPS data to\n" 
        "    --no-gps                     use fixed input lat,lon as information\n"
        "    --lat <lat>                  fixed latitude in DD format\n"
        "    --lon <lon>                  fixed longitude in DD format\n"
        "    --help                       print usage\n"
        "    --no-stdout                  no stdout\n"
        "\n"
        "Returns:\n"
        "    0 - success\n"
        "    1 - failure\n"
        "\n"
        "Example:\n"
        "    gps_util -h localhost -p 8888 -f dms -o file.json\n"
        "    gps_util -h localhost -p 8888 -f aprs --aprs-comment \"Downtown Bellevue fill-in Digipeater\" --aprs-symbol \"#\" --aprs-symbol-table-id \"I\"\n"
        "\n"
        "\n";
    printf("%s", usage.c_str());
}

std::string to_lower(const std::string& s)
{
    std::string lower_s = s;
    std::transform(lower_s.begin(), lower_s.end(), lower_s.begin(), ::tolower);
    return lower_s;
}

bool try_parse_bool(const std::string& s, bool& b)
{
    std::string lower_s = to_lower(s);

    if (lower_s == "true")
    {
        b = true;
        return true;
    }
    else if (lower_s == "false")
    {
        b = false;
        return true;
    }

    return false;
}

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
    else if (pos_str == "aprs")
        return position_print_format::aprs;
    return position_print_format::dd;
}

void print_position(position_print_format print_fmt, const gnss_info& gnss_info)
{
    position_dd dd = { gnss_info.lat, gnss_info.lon };
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
        pos_display = format(position_ddm(dd), position_ddm_short_format);
    }

    printf("%s, %s\n", pos_display.lat.c_str(), pos_display.lon.c_str());
}

int write_position(const std::string& filename, const gnss_info& info)
{
    std::string json = to_json(info);

    std::ofstream output_file(filename);
    if (!output_file)
    {
        return 1;
    }

    output_file << json;
    output_file.close();

    return 0;
}

std::string encode_aprs_position_packet_no_time(const std::string& symbol, const std::string& symbol_table, const std::string& comment, double lat, double lon) 
{
    // 
    //  Data Format:
    // 
    //     !   Lat  Sym  Lon  Sym Code   Comment
    //     =
    //    ------------------------------------------
    //     1    8    1    9      1        0-43
    //
    //  Examples:
    //
    //    !4903.50N/07201.75W-Test 001234
    //    !4903.50N/07201.75W-Test /A=001234
    //    !49  .  N/072  .  W-
    //

    position_dd dd { lat, lon };
    position_display_string ddm_short_display = format(position_ddm(dd), position_ddm_short_format);

    std::string message;

    message.append("!");
    message.append(ddm_short_display.lat);
    message.append(symbol_table);
    message.append(ddm_short_display.lon);
    message.append(symbol);
    message.append(comment);

    return message;
}

std::string encode_aprs_position_packet_no_time(const args& args, double lat, double lon) 
{
    return encode_aprs_position_packet_no_time(args.aprs_symbol, args.aprs_symbol_table, args.aprs_comment, lat, lon);
}

std::string encode_aprs_position_packet_no_time(const args& args) 
{
    return encode_aprs_position_packet_no_time(args.aprs_symbol, args.aprs_symbol_table, args.aprs_comment, args.lat, args.lon);
}

std::string encode_aprs_position_packet(const std::string& symbol, const std::string& symbol_table, const std::string& comment, const gnss_info& gnss_info) 
{
    // 
    //  Data Format:
    // 
    //     /   Time  Lat   Sym  Lon  Sym Code   Comment
    //     @
    //    -----------------------------------------------
    //     1    7     8     1    9      1        0-43
    //
    //  Examples:
    //
    //    /092345z4903.50N/07201.75W>Test1234
    //    @092345/4903.50N/07201.75W>Test1234
    //

    position_dd dd { gnss_info.lat, gnss_info.lon };
    position_display_string ddm_short_display = format(position_ddm(dd), position_ddm_short_format);

    std::string message;

    message.append("/");
    message.append(format_two_digits_string(gnss_info.time_utc.day));
    message.append(format_two_digits_string(gnss_info.time_utc.hour));
    message.append(format_two_digits_string(gnss_info.time_utc.minute));
    message.append("z");
    message.append(ddm_short_display.lat);
    message.append(symbol_table);
    message.append(ddm_short_display.lon);
    message.append(symbol);
    message.append(comment);

    return message;
}

std::string encode_aprs_position_packet(const args& args, const gnss_info& gnss_info)
{
    return encode_aprs_position_packet(args.aprs_symbol, args.aprs_symbol_table, args.aprs_comment, gnss_info);
}

void print_aprs_position_packet(const args& args, const gnss_info& gnss_info)
{
    std::string packet = args.no_gps ? encode_aprs_position_packet_no_time(args) :
        encode_aprs_position_packet(args, gnss_info);
    printf("%s\n", packet.c_str());
}

std::string format_two_digits_string(int number)
{
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << number;
    return oss.str();
}

bool try_get_gps_info(const args& args, gnss_info& info)
{
    gpsd_client s;
    bool result = false;
    if (s.open(args.host_name, args.port))
    {
        do
        {
            result = s.try_get_gps_info(info, gnss_include_info::all);
        }
        while (false);
        s.close();
    }
    return result;
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

    if (args.command_line_has_errors)
    {
        if (!args.no_stdout)
        {
            printf("%s\n\n", args.command_line_error.c_str());
            print_usage();
        }
        return 1;
    }

    gnss_info info;

    if (try_get_gps_info(args, info))
    {
        if (!args.no_stdout)
        {
            if (args.format == position_print_format::aprs)
            {
                print_aprs_position_packet(args, info);
            }
            else
            {
                print_position(args.format, info);
            }
        }
        if (!args.output_file.empty())
        {
            return write_position(args.output_file, info);
        }

        return 0;
    }

    return 1;
}
