// Distributed under the MIT license
// Developed by Anyar Inc.

#include "glaze/csv/csv.hpp"
#include "glaze/csv/recorder.h"

#define BOOST_UT_DISABLE_MODULE 1

#include "boost/ut.hpp"


#include <cmath>
#include <deque>
#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include <variant>
#include <numeric>
#include <sstream>

using namespace boost::ut;

int main()
{
    suite csv_write = [] {

        "rowwise_to_file"_test = [] {

            std::vector<double> x, y;
            std::deque<bool> z;
            for (auto i = 0; i < 100; ++i) {
                const auto a = static_cast<double>(i);
                x.emplace_back(a);
                y.emplace_back(std::sin(a));
                z.emplace_back(i % 2 == 0);
            }

            csv::to_file("rowwise_to_file_test", "x", x, "y", y, "z", z);
        };

        "colwise_to_file"_test = [] {
            std::vector<double> x, y;
            std::deque<bool> z;
            for (auto i = 0; i < 100; ++i) {
                const auto a = static_cast<double>(i);
                x.emplace_back(a);
                y.emplace_back(std::sin(a));
                z.emplace_back(i % 2 == 0);
            }

            csv::to_file<false>("colwise_to_file_test", "z", z, "y", y, "x", x);
        };

        "vector_to_buffer"_test = [] {
            std::vector<double> data(25);
            std::iota(data.begin(), data.end(), 1);

            std::string buffer;
            csv::write(buffer, "data", data);
        };

        "deque_to_buffer"_test = [] {
            std::deque<double> data(25);
            std::iota(data.begin(), data.end(), 1);

            std::string buffer;
            csv::write(buffer, "data", data);
        };

        "array_to_buffer"_test = [] {
            std::array<double, 25> data;
            std::iota(data.begin(), data.end(), 1);

            std::string buffer;
            csv::write(buffer, "data", data);
        };

        "rowwise_map_to_buffer"_test = [] {
            std::vector<double> x, y;
            for (auto i = 0; i < 100; ++i) {
                const auto a = static_cast<double>(i);
                x.emplace_back(a);
                y.emplace_back(std::sin(a));
            }

            std::map<std::string, std::vector<double>> data;
            data["x"] = x;
            data["y"] = y;

            std::string buffer;
            csv::write(buffer, data);
        };

        "colwise_map_to_buffer"_test = [] {
            std::vector<double> x, y;
            for (auto i = 0; i < 100; ++i) {
                const auto a = static_cast<double>(i);
                x.emplace_back(a);
                y.emplace_back(std::sin(a));
            }

            std::map<std::string, std::vector<double>> data;
            data["x"] = x;
            data["y"] = y;

            std::string buffer;
            csv::write<false>(buffer, data);
        };

        "map_mismatch"_test = [] {
            std::map<std::string, std::vector<int>> data;

            for (int i = 0; i < 100; i++)
            {
                data["x"].emplace_back(i);
                if (i % 2)
                    data["y"].emplace_back(i);
            }

            try
            {
                std::string buffer;
                csv::write(buffer, data);

                expect(false);
            }
            catch (std::exception&)
            {
                expect(true);
            }
        };
    };

    suite csv_read = [] {

        "rowwise_from_file"_test = [] {
            std::vector<double> x, y;
            std::deque<bool> z;

            csv::from_file("rowwise_to_file_test", x, y, z);

        };

        "colwise_from_file"_test = [] {
            std::vector<double> x, y;
            std::deque<bool> z;

            csv::from_file<false>("colwise_to_file_test", z, y, x);

        };

        // more complicated. needs a small discussion on expectations
        //"from_file_to_map"_test = [] {

        //};

        "partial_data"_test = [] {
            std::vector<double> z;
            csv::from_file<false>("colwise_to_file_test", z);
        };

        "wrong_type"_test = [] {
            std::vector<std::string> letters = {"a", "b", "c", "d", "e", 
                                                   "f", "g", "h", "i", "j", 
                                                   "k", "l", "m", "n", "o"};

            csv::to_file("letters_file", "letters", letters);

            std::vector<double> not_letters;

            try
            {
                csv::from_file("letters_file", not_letters);

                expect(false);
            }
            catch (std::exception&)
            {
                expect(true);
            }
        };

    };

    suite csv_recorder = [] {

        "recorder_to_file"_test = [] {

            recorder<std::variant<double, float>> rec;

            double testvar = 0.0;

            rec.register_variable("test", &testvar);

            for (int i = 0; i < 10; i++)
            {
                testvar += 1.5;
                rec.update();
            }

            csv::to_file("recorder_out", rec);
        };

    };

    return 0;
}
