// Feeds arbitrary bytes to BrainFlow's CSV/TSV reader (get_num_elements_in_file / read_file),
// and round-trips parsed values through write_file.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "fuzz_common.h"

extern "C" int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
    quiet_logs ();
    const char *tmp = getenv ("TMPDIR");
    std::string path = std::string (tmp && *tmp ? tmp : "/tmp") + "/bf_fuzz_read_XXXXXX";
    std::vector<char> tmpl (path.begin (), path.end ());
    tmpl.push_back ('\0');
    int fd = mkstemp (tmpl.data ());
    if (fd < 0)
        return -1;
    size_t off = 0;
    while (off < size)
    {
        ssize_t w = write (fd, data + off, size - off);
        if (w <= 0)
            break;
        off += (size_t)w;
    }
    close (fd);

    int count = 0;
    if (get_num_elements_in_file (tmpl.data (), &count) == 0 && count > 0 && count < (1 << 20))
    {
        std::vector<double> values ((size_t)count);
        int rows = 0, cols = 0;
        int res = read_file (values.data (), &rows, &cols, tmpl.data (), count);
        if (res == 0 && rows > 0 && cols > 0)
        {
            std::string out = std::string (tmpl.data ()) + ".out";
            if (write_file (values.data (), rows, cols, out.c_str (), "w") == 0)
            {
                std::vector<double> again ((size_t)count);
                int r2 = 0, c2 = 0;
                read_file (again.data (), &r2, &c2, out.c_str (), count);
            }
            unlink ((std::string (tmpl.data ()) + ".out").c_str ());
        }
        std::vector<double> small (1);
        int r3 = 0, c3 = 0;
        read_file (small.data (), &r3, &c3, tmpl.data (), 1);
    }
    unlink (tmpl.data ());
    return 0;
}
