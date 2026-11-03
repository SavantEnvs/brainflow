// Known-answer probe for the data_handler library: prints one "name: value" line per check.
// mayhem/test.sh compares every line against hard-coded expected values.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "data_handler.h"

static void show (const char *name, const std::vector<double> &v, int count = -1)
{
    printf ("%s:", name);
    size_t n = count < 0 ? v.size () : (size_t)count;
    for (size_t i = 0; i < n; i++)
        printf (" %.4f", std::fabs (v[i]) < 5e-5 ? 0.0 : v[i]);
    printf ("\n");
}

int main ()
{
    set_log_level_data_handler (6);

    char ver[64] = {0};
    int vlen = 0;
    int res = get_version_data_handler (ver, &vlen, 64);
    printf ("version: %d %s\n", res, ver);

    int p2 = 0;
    res = get_nearest_power_of_two (1000, &p2);
    printf ("pow2: %d %d\n", res, p2);
    res = get_nearest_power_of_two (0, &p2);
    printf ("pow2_zero: %d\n", res);

    {
        std::vector<double> x = {1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<double> re (5), im (5), back (8);
        res = perform_fft (x.data (), 8, 0, re.data (), im.data ());
        printf ("fft_res: %d\n", res);
        show ("fft_re", re);
        show ("fft_im", im);
        res = perform_ifft (re.data (), im.data (), 8, back.data ());
        printf ("ifft_res: %d\n", res);
        show ("ifft", back);
        std::vector<double> bad (5);
        printf ("fft_odd_len: %d\n", perform_fft (x.data (), 7, 0, re.data (), im.data ()));
        printf ("fft_null: %d\n", perform_fft (nullptr, 8, 0, re.data (), im.data ()));
    }
    {
        std::vector<double> w (8);
        res = get_window (1, 8, w.data ());
        printf ("hann_res: %d\n", res);
        show ("hann", w);
        res = get_window (2, 8, w.data ());
        show ("hamming", w);
        printf ("window_bad: %d\n", get_window (9, 8, w.data ()));
    }
    {
        std::vector<double> x = {1, 2, 3, 4, 5};
        res = detrend (x.data (), 5, 1);
        printf ("detrend_const_res: %d\n", res);
        show ("detrend_const", x);
        std::vector<double> y = {2, 4, 6, 8, 10, 12};
        detrend (y.data (), 6, 2);
        show ("detrend_linear", y);
    }
    {
        std::vector<double> x = {1, 2, 3, 4, 5, 6};
        std::vector<double> out (3);
        res = perform_downsampling (x.data (), 6, 2, 0, out.data ());
        printf ("down_mean_res: %d\n", res);
        show ("down_mean", out);
        res = perform_downsampling (x.data (), 6, 3, 1, out.data ());
        show ("down_median", out, 2);
        std::vector<double> r = {1, 2, 3, 4, 5, 6};
        res = perform_rolling_filter (r.data (), 6, 3, 0);
        printf ("rolling_res: %d\n", res);
        show ("rolling_mean", r);
        std::vector<double> d = {1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<double> dout (4);
        res = perform_decimation (d.data (), 8, 2, dout.data ());
        printf ("decim_res: %d\n", res);
        show ("decim_len4_finite", std::vector<double> {std::isfinite (dout[0]) && std::isfinite (dout[3]) ? 1.0 : 0.0});
    }
    {
        std::vector<double> x = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        double sd = 0;
        res = calc_stddev (x.data (), 0, 9, &sd);
        printf ("stddev: %d %.4f\n", res, sd);
        printf ("stddev_bad: %d\n", calc_stddev (x.data (), 5, 2, &sd));
    }
    {
        // lowpass on DC keeps DC; lowpass on a fast sine removes it; highpass removes DC
        const int n = 1000, fs = 250;
        std::vector<double> dc (n, 5.0);
        res = perform_lowpass (dc.data (), n, fs, 20.0, 4, 0, 0.0);
        printf ("lowpass_res: %d\n", res);
        printf ("lowpass_dc: %.3f\n", dc[n / 2]);
        std::vector<double> hp (n, 5.0);
        perform_highpass (hp.data (), n, fs, 20.0, 4, 0, 0.0);
        printf ("highpass_dc: %.3f\n", std::fabs (hp[n / 2]) < 1e-3 ? 0.0 : hp[n / 2]);
        std::vector<double> sine (n);
        for (int i = 0; i < n; i++)
            sine[i] = std::sin (2.0 * M_PI * 60.0 * i / fs);
        perform_lowpass (sine.data (), n, fs, 5.0, 4, 0, 0.0);
        double peak = 0;
        for (int i = n / 4; i < 3 * n / 4; i++)
            peak = std::fmax (peak, std::fabs (sine[i]));
        printf ("lowpass_attenuates: %d\n", peak < 0.01);
        printf ("lowpass_bad_cutoff: %d\n", perform_lowpass (dc.data (), n, fs, -1.0, 4, 0, 0.0));
        std::vector<double> bp (n);
        for (int i = 0; i < n; i++)
            bp[i] = std::sin (2.0 * M_PI * 10.0 * i / fs);
        perform_bandpass (bp.data (), n, fs, 5.0, 20.0, 4, 3, 0.0);
        double bpk = 0;
        for (int i = n / 4; i < 3 * n / 4; i++)
            bpk = std::fmax (bpk, std::fabs (bp[i]));
        printf ("bandpass_passes: %d\n", bpk > 0.8 && bpk < 1.2);
    }
    {
        // 32 Hz sine sampled at 256 Hz: PSD peak must sit at 32 Hz
        const int n = 256, fs = 256;
        std::vector<double> x (n);
        for (int i = 0; i < n; i++)
            x[i] = std::sin (2.0 * M_PI * 32.0 * i / fs);
        std::vector<double> ampl (n / 2 + 1), freq (n / 2 + 1);
        res = get_psd (x.data (), n, fs, 1, ampl.data (), freq.data ());
        printf ("psd_res: %d\n", res);
        int best = 0;
        for (int i = 1; i < n / 2 + 1; i++)
            if (ampl[i] > ampl[best])
                best = i;
        printf ("psd_peak_freq: %.3f\n", freq[best]);
        double bp = 0, off = 0;
        get_band_power (ampl.data (), freq.data (), n / 2 + 1, 30.0, 34.0, &bp);
        get_band_power (ampl.data (), freq.data (), n / 2 + 1, 60.0, 64.0, &off);
        printf ("band_power_dominant: %d\n", bp > 100.0 * off && bp > 0.0);
        std::vector<double> wa (33), wf (33);
        res = get_psd_welch (x.data (), n, 64, 32, fs, 1, wa.data (), wf.data ());
        printf ("welch_res: %d\n", res);
        int wb = 0;
        for (int i = 1; i < 33; i++)
            if (wa[i] > wa[wb])
                wb = i;
        printf ("welch_peak_freq: %.3f\n", wf[wb]);
    }
    {
        std::vector<double> x = {1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<double> out (8 + 2 * 1 * 41);
        std::vector<int> lens (2);
        res = perform_wavelet_transform (x.data (), 8, 0, 1, 0, out.data (), lens.data ());
        printf ("haar_res: %d lens %d %d\n", res, lens[0], lens[1]);
        show ("haar_coeffs", out, lens[0] + lens[1]);
        std::vector<double> back (8);
        res = perform_inverse_wavelet_transform (out.data (), 8, 0, 1, 0, lens.data (), back.data ());
        printf ("haar_inv_res: %d\n", res);
        show ("haar_inv", back);
    }
    {
        const double m[6] = {1.5, -2.0, 3.25, 4.0, 5.5, -6.75};
        const char *name = "kat_probe_matrix.csv";
        res = write_file (m, 2, 3, name, "w");
        printf ("write_res: %d\n", res);
        int count = 0;
        res = get_num_elements_in_file (name, &count);
        printf ("count: %d %d\n", res, count);
        std::vector<double> back (6);
        int rows = 0, cols = 0;
        res = read_file (back.data (), &rows, &cols, name, 6);
        printf ("read_res: %d rows %d cols %d\n", res, rows, cols);
        show ("read_back", back);
        printf ("read_short: %d\n", read_file (back.data (), &rows, &cols, name, 5));
        remove (name);
        printf ("read_missing: %d\n", read_file (back.data (), &rows, &cols, name, 6));
    }
    {
        std::vector<double> d (100, 0.0);
        double out = -1;
        res = get_flatline_percentage (d.data (), 100, 0.0, &out);
        printf ("flatline: %d %.2f\n", res, out);
        for (int i = 0; i < 100; i++)
            d[i] = i;
        res = get_clipping_percentage (d.data (), 100, 10.0, 89.0, &out);
        printf ("clipping: %d %.2f\n", res, out);
    }
    return 0;
}
