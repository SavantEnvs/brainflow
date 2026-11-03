// Frequency-domain processing: FFT/IFFT, PSD, Welch, band powers, windows, PPG metrics.
#include <algorithm>

#include "fuzz_common.h"

static void fft_ops (FuzzedDataProvider &p)
{
    std::vector<double> sig = take_signal (p, 512);
    int n = (int)sig.size ();
    int win = p.ConsumeIntegralInRange<int> (-1, 4);
    size_t half = (size_t)(n > 0 ? n / 2 + 1 : 1);
    std::vector<double> re (half), im (half);
    std::vector<double> back ((size_t)n + 1);
    if (perform_fft (sig.data (), n, win, re.data (), im.data ()) == 0)
        perform_ifft (re.data (), im.data (), n, back.data ());
    std::vector<double> bad_re = take_signal (p, 300), bad_im = take_signal (p, 300);
    int m = (int)std::min (bad_re.size (), bad_im.size ());
    std::vector<double> restored ((size_t)m + 1);
    perform_ifft (bad_re.data (), bad_im.data (), m, restored.data ());
    int pow2 = 0;
    get_nearest_power_of_two (p.ConsumeIntegral<int> (), &pow2);
}

static void psd_ops (FuzzedDataProvider &p)
{
    std::vector<double> sig = take_signal (p, 1024);
    int n = (int)sig.size ();
    int fs = p.ConsumeIntegralInRange<int> (-1, 2000);
    int win = p.ConsumeIntegralInRange<int> (-1, 4);
    if (p.ConsumeBool ())
    {
        size_t half = (size_t)(n > 0 ? n / 2 + 1 : 1);
        std::vector<double> ampl (half), freq (half);
        if (get_psd (sig.data (), n, fs, win, ampl.data (), freq.data ()) == 0)
        {
            double bp = 0;
            get_band_power (ampl.data (), freq.data (), (int)half, take_param (p, -1, 100),
                take_param (p, -1, 100), &bp);
        }
    }
    else
    {
        int nfft = 1 << p.ConsumeIntegralInRange<int> (0, 9);
        if (p.ConsumeIntegralInRange<int> (0, 7) == 0)
            nfft = p.ConsumeIntegralInRange<int> (-2, 600);
        int overlap = p.ConsumeIntegralInRange<int> (-1, nfft + 1);
        size_t half = (size_t)(nfft > 0 ? nfft / 2 + 1 : 1);
        std::vector<double> ampl (half), freq (half);
        get_psd_welch (sig.data (), n, nfft, overlap, fs, win, ampl.data (), freq.data ());
    }
}

static void band_ops (FuzzedDataProvider &p)
{
    int rows = p.ConsumeIntegralInRange<int> (-1, 6);
    int cols = p.ConsumeIntegralInRange<int> (-1, 300);
    int bands = p.ConsumeIntegralInRange<int> (-1, 6);
    size_t total = (size_t)std::max (rows, 0) * (size_t)std::max (cols, 0);
    std::vector<double> raw = take_signal (p, 1);
    raw.clear ();
    for (size_t i = 0; i < total; i++)
        raw.push_back (p.ConsumeFloatingPointInRange<double> (-500.0, 500.0));
    std::vector<double> start ((size_t)std::max (bands, 0) + 1), stop ((size_t)std::max (bands, 0) + 1);
    for (size_t i = 0; i < start.size (); i++)
    {
        start[i] = take_param (p, 0, 60);
        stop[i] = take_param (p, 0, 120);
    }
    int fs = p.ConsumeIntegralInRange<int> (-1, 1000);
    int apply = p.ConsumeIntegralInRange<int> (0, 1);
    std::vector<double> avg (start.size ()), sd (start.size ());
    if (p.ConsumeBool ())
    {
        get_custom_band_powers (raw.data (), rows, cols, start.data (), stop.data (), bands, fs,
            apply, avg.data (), sd.data ());
    }
    else
    {
        int nfft = p.ConsumeBool () ? 0 : p.ConsumeIntegralInRange<int> (-1, 600);
        double lo = take_param (p, 0, 10), hi = take_param (p, 0, 100);
        int mains = p.ConsumeIntegralInRange<int> (-2, 4);
        int det = p.ConsumeIntegralInRange<int> (-1, 3);
        int eff = 0, edge = 0;
        get_band_power_settings (fs, apply, nfft, lo, hi, mains, &eff, &edge);
        get_custom_band_powers_with_options (raw.data (), rows, cols, start.data (), stop.data (),
            bands, fs, apply, nfft, lo, hi, mains, det, avg.data (), sd.data ());
    }
}

static void window_and_ppg (FuzzedDataProvider &p)
{
    if (p.ConsumeBool ())
    {
        int len = p.ConsumeIntegralInRange<int> (-1, 600);
        std::vector<double> w ((size_t)std::max (len, 0) + 1);
        get_window (p.ConsumeIntegralInRange<int> (-1, 4), len, w.data ());
        return;
    }
    std::vector<double> ir = take_signal (p, 1024), red = take_signal (p, 1024);
    int n = (int)std::min (ir.size (), red.size ());
    int fs = p.ConsumeIntegralInRange<int> (-1, 500);
    double out = 0;
    if (p.ConsumeBool ())
        get_oxygen_level (ir.data (), red.data (), n, fs, take_param (p, -5, 5), take_param (p, -5, 5),
            take_param (p, -5, 5), &out);
    else
        get_heart_rate (ir.data (), red.data (), n, fs, 1 << p.ConsumeIntegralInRange<int> (0, 10), &out);
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
    quiet_logs ();
    FuzzedDataProvider p (data, size);
    switch (p.ConsumeIntegralInRange<int> (0, 3))
    {
        case 0:
            fft_ops (p);
            break;
        case 1:
            psd_ops (p);
            break;
        case 2:
            band_ops (p);
            break;
        default:
            window_and_ppg (p);
            break;
    }
    return 0;
}
