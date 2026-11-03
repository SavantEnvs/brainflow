// Time-domain processing: IIR filters, noise removal, rolling filter, resampling, detrend,
// windows, streaming filters, signal-quality metrics.
#include "fuzz_common.h"

static void filters (FuzzedDataProvider &p)
{
    std::vector<double> sig = take_signal (p, 512);
    int n = (int)sig.size ();
    int fs = p.ConsumeIntegralInRange<int> (-2, 4000);
    int order = p.ConsumeIntegralInRange<int> (-1, 12);
    int type = p.ConsumeIntegralInRange<int> (-1, 6);
    double ripple = take_param (p, -1.0, 6.0);
    double a = take_param (p, -10.0, 2000.0);
    double b = take_param (p, -10.0, 2000.0);
    switch (p.ConsumeIntegralInRange<int> (0, 4))
    {
        case 0:
            perform_lowpass (sig.data (), n, fs, a, order, type, ripple);
            break;
        case 1:
            perform_highpass (sig.data (), n, fs, a, order, type, ripple);
            break;
        case 2:
            perform_bandpass (sig.data (), n, fs, a, b, order, type, ripple);
            break;
        case 3:
            perform_bandstop (sig.data (), n, fs, a, b, order, type, ripple);
            break;
        default:
            remove_environmental_noise (sig.data (), n, fs, p.ConsumeIntegralInRange<int> (-1, 3));
            break;
    }
}

static void resampling (FuzzedDataProvider &p)
{
    std::vector<double> sig = take_signal (p, 512);
    int n = (int)sig.size ();
    int period = p.ConsumeIntegralInRange<int> (-1, 40);
    int agg = p.ConsumeIntegralInRange<int> (-1, 3);
    switch (p.ConsumeIntegralInRange<int> (0, 3))
    {
        case 0:
            perform_rolling_filter (sig.data (), n, period, agg);
            break;
        case 1:
        {
            std::vector<double> out ((size_t)(period > 0 ? n / period : 0) + 1);
            perform_downsampling (sig.data (), n, period, agg, out.data ());
            break;
        }
        case 2:
        {
            std::vector<double> out ((size_t)(period > 0 ? n / period : 0) + 1);
            perform_decimation (sig.data (), n, period, out.data ());
            break;
        }
        default:
            detrend (sig.data (), n, p.ConsumeIntegralInRange<int> (-1, 3));
            break;
    }
}

static void streaming (FuzzedDataProvider &p)
{
    int kind = p.ConsumeIntegralInRange<int> (-1, 4);
    int fs = p.ConsumeIntegralInRange<int> (-2, 2000);
    double lo = take_param (p, -5.0, 1000.0);
    double hi = take_param (p, -5.0, 1000.0);
    int order = p.ConsumeIntegralInRange<int> (-1, 10);
    int type = p.ConsumeIntegralInRange<int> (-1, 6);
    double ripple = take_param (p, -1.0, 6.0);
    int edge = 0;
    get_filter_settling_samples (kind, fs, lo, hi, order, type, ripple, &edge);
    int handle = -1;
    if (create_streaming_filter (kind, fs, lo, hi, order, type, ripple, &handle) != 0)
        return;
    int chunks = p.ConsumeIntegralInRange<int> (1, 4);
    for (int i = 0; i < chunks; i++)
    {
        std::vector<double> sig = take_signal (p, 128);
        perform_streaming_filter (handle, sig.data (), (int)sig.size ());
        if (p.ConsumeBool ())
            reset_streaming_filter (handle);
    }
    release_streaming_filter (handle);
    perform_streaming_filter (handle, nullptr, 0);
}

static void metrics (FuzzedDataProvider &p)
{
    std::vector<double> sig = take_signal (p, 512);
    int n = (int)sig.size ();
    double out = 0;
    switch (p.ConsumeIntegralInRange<int> (0, 5))
    {
        case 0:
            get_railed_percentage (sig.data (), n, p.ConsumeIntegralInRange<int> (-1, 64), &out);
            break;
        case 1:
            get_clipping_percentage (
                sig.data (), n, take_param (p, -1000, 0), take_param (p, 0, 1000), &out);
            break;
        case 2:
            get_flatline_percentage (sig.data (), n, take_param (p, -1.0, 10.0), &out);
            break;
        case 3:
        {
            // calc_stddev has no length argument: [start, end) must lie inside the buffer
            int s = p.ConsumeIntegralInRange<int> (-2, n);
            int e = p.ConsumeIntegralInRange<int> (-2, n);
            calc_stddev (sig.data (), s, e, &out);
            break;
        }
        case 4:
        {
            std::vector<double> o ((size_t)n + 1);
            detect_peaks_z_score (sig.data (), n, p.ConsumeIntegralInRange<int> (-1, 64),
                take_param (p, -1.0, 8.0), take_param (p, -0.5, 1.5), o.data ());
            break;
        }
        default:
        {
            std::vector<double> y = take_signal (p, 512), z = take_signal (p, 512);
            int len = (int)std::min (std::min (y.size (), z.size ()), sig.size ());
            double nv = take_param (p, 0, 1);
            int fs = p.ConsumeIntegralInRange<int> (-1, 500);
            int period = p.ConsumeIntegralInRange<int> (-1, 100);
            get_activity_index (sig.data (), y.data (), z.data (), len, fs, period, nv, nv, nv, &out);
            break;
        }
    }
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
    quiet_logs ();
    FuzzedDataProvider p (data, size);
    switch (p.ConsumeIntegralInRange<int> (0, 3))
    {
        case 0:
            filters (p);
            break;
        case 1:
            resampling (p);
            break;
        case 2:
            streaming (p);
            break;
        default:
            metrics (p);
            break;
    }
    return 0;
}
