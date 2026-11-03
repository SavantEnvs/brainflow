#pragma once

#include <fuzzer/FuzzedDataProvider.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "data_handler.h"

// Up to max_len doubles taken from the input (raw bit patterns, so NaN/Inf/denormals occur).
static inline std::vector<double> take_signal (FuzzedDataProvider &p, size_t max_len)
{
    size_t n = p.ConsumeIntegralInRange<size_t> (0, max_len);
    std::vector<double> v;
    v.reserve (n);
    for (size_t i = 0; i < n; i++)
    {
        // mostly well-behaved values so deeper numeric code runs, occasionally arbitrary doubles
        if (p.ConsumeIntegralInRange<int> (0, 15) == 0)
            v.push_back (p.ConsumeFloatingPoint<double> ());
        else
            v.push_back (p.ConsumeFloatingPointInRange<double> (-1000.0, 1000.0));
    }
    return v;
}

static inline double take_param (FuzzedDataProvider &p, double lo, double hi)
{
    if (p.ConsumeIntegralInRange<int> (0, 31) == 0)
        return p.ConsumeFloatingPoint<double> ();
    return p.ConsumeFloatingPointInRange<double> (lo, hi);
}

static inline void quiet_logs ()
{
    static bool done = false;
    if (!done)
    {
        set_log_level_data_handler (6);
        done = true;
    }
}
