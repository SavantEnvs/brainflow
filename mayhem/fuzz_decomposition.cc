// Wavelet transforms/denoising, ICA and CSP decompositions.
#include <algorithm>

#include "fuzz_common.h"

static void wavelets (FuzzedDataProvider &p)
{
    std::vector<double> sig = take_signal (p, 512);
    int n = (int)sig.size ();
    int wavelet = p.ConsumeIntegralInRange<int> (-1, 45);
    int level = p.ConsumeIntegralInRange<int> (-1, 8);
    int ext = p.ConsumeIntegralInRange<int> (-1, 2);
    switch (p.ConsumeIntegralInRange<int> (0, 4))
    {
        case 0:
        {
            size_t lev = (size_t)std::max (level, 0);
            std::vector<double> out ((size_t)n + 2 * lev * 41 + 1);
            std::vector<int> lens (lev + 2);
            if (perform_wavelet_transform (
                    sig.data (), n, wavelet, level, ext, out.data (), lens.data ()) == 0)
            {
                std::vector<double> rest ((size_t)n + 1);
                perform_inverse_wavelet_transform (
                    out.data (), n, wavelet, level, ext, lens.data (), rest.data ());
                int total = 0;
                for (int i = 0; i < level + 1; i++)
                    total += lens[i];
                perform_inverse_wavelet_transform_checked (out.data (), total, n, wavelet, level,
                    ext, lens.data (), level + 1, rest.data (), n);
            }
            break;
        }
        case 1:
        {
            // coefficient/length arrays come straight from the input, sizes declared honestly
            std::vector<double> coeffs = take_signal (p, 300);
            std::vector<int> lens;
            int cnt = p.ConsumeIntegralInRange<int> (0, 10);
            for (int i = 0; i < cnt; i++)
                lens.push_back (p.ConsumeIntegralInRange<int> (-5, 300));
            std::vector<double> out ((size_t)n + 1);
            perform_inverse_wavelet_transform_checked (coeffs.data (), (int)coeffs.size (), n,
                wavelet, level, ext, lens.data (), (int)lens.size (), out.data (), n);
            break;
        }
        case 2:
            perform_wavelet_denoising (sig.data (), n, wavelet, level,
                p.ConsumeIntegralInRange<int> (-1, 2), p.ConsumeIntegralInRange<int> (-1, 2), ext,
                p.ConsumeIntegralInRange<int> (-1, 2));
            break;
        case 3:
        {
            std::vector<double> out ((size_t)n + 1);
            restore_data_from_wavelet_detailed_coeffs (sig.data (), n, wavelet, level,
                p.ConsumeIntegralInRange<int> (-1, 9), out.data ());
            break;
        }
        default:
            break;
    }
}

static void ica (FuzzedDataProvider &p)
{
    int rows = p.ConsumeIntegralInRange<int> (-1, 8);
    int cols = p.ConsumeIntegralInRange<int> (-1, 64);
    int nc = p.ConsumeIntegralInRange<int> (-1, 8);
    size_t r = (size_t)std::max (rows, 0), c = (size_t)std::max (cols, 0);
    size_t k = (size_t)std::max (nc, 0);
    std::vector<double> data (r * c);
    for (double &v : data)
        v = p.ConsumeFloatingPointInRange<double> (-100.0, 100.0);
    if (!data.empty () && p.ConsumeIntegralInRange<int> (0, 15) == 0)
        data[p.ConsumeIntegralInRange<size_t> (0, data.size () - 1)] = p.ConsumeFloatingPoint<double> ();
    std::vector<double> w (k * k + 1), kk (r * k + 1), a (k * r + 1), s (c * k + 1);
    if (p.ConsumeBool ())
        perform_ica (data.data (), rows, cols, nc, w.data (), kk.data (), a.data (), s.data ());
    else
        perform_ica_with_options (data.data (), rows, cols, nc, w.data (), kk.data (), a.data (),
            s.data (), p.ConsumeIntegralInRange<int> (-1, 50), take_param (p, -0.1, 1.0),
            p.ConsumeIntegral<int> ());
}

static void csp (FuzzedDataProvider &p)
{
    int epochs = p.ConsumeIntegralInRange<int> (-1, 6);
    int ch = p.ConsumeIntegralInRange<int> (-1, 5);
    int times = p.ConsumeIntegralInRange<int> (-1, 40);
    size_t e = (size_t)std::max (epochs, 0), c = (size_t)std::max (ch, 0), t = (size_t)std::max (times, 0);
    std::vector<double> data (e * c * t), labels (e);
    for (double &v : data)
        v = p.ConsumeFloatingPointInRange<double> (-50.0, 50.0);
    for (double &l : labels)
        l = p.ConsumeIntegralInRange<int> (-1, 2);
    std::vector<double> w (c * c + 1), d (c + 1);
    get_csp (data.data (), labels.data (), epochs, ch, times, w.data (), d.data ());
}

extern "C" int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
    quiet_logs ();
    FuzzedDataProvider p (data, size);
    switch (p.ConsumeIntegralInRange<int> (0, 2))
    {
        case 0:
            wavelets (p);
            break;
        case 1:
            ica (p);
            break;
        default:
            csp (p);
            break;
    }
    return 0;
}
