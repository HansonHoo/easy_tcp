#ifndef _CELL_TIME_STAMP_HPP_
#define _CELL_TIME_STAMP_HPP_

#include <chrono>
using namespace std::chrono;

class CELLTimestamp {
public:
    CELLTimestamp() {
        update();
    }

    ~CELLTimestamp() {}

    void update() {
        begin_ = high_resolution_clock::now();
    }

    /**
     * 获取当前秒
     */
    double get_elapsed_second() {
        return get_elapsed_time_in_microsec() * 0.000001;
    }

    /**
     * 获取毫秒
     */
    double get_elapsed_time_in_millisec() {
        return this->get_elapsed_time_in_microsec() * 0.001;
    }

    /**
     * 获取微秒
     */
    long long get_elapsed_time_in_microsec() {
        return duration_cast<microseconds>(high_resolution_clock::now() - begin_).count();
    }

protected:
    time_point<high_resolution_clock> begin_;
};

#endif