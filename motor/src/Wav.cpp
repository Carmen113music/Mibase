#include "mibase/Wav.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mibase
{
    namespace
    {
        void posaU32 (char* d, std::uint32_t v) { for (int i = 0; i < 4; ++i) d[i] = (char) ((v >> (8 * i)) & 0xff); }
        void posaU16 (char* d, std::uint16_t v) { d[0] = (char) (v & 0xff); d[1] = (char) (v >> 8); }
        std::uint32_t llegeixU32 (const unsigned char* d) { return (std::uint32_t) d[0] | ((std::uint32_t) d[1] << 8) | ((std::uint32_t) d[2] << 16) | ((std::uint32_t) d[3] << 24); }
        std::uint16_t llegeixU16 (const unsigned char* d) { return (std::uint16_t) (d[0] | (d[1] << 8)); }

        void capcalera (char* h, int canals, double fs, int bits, long long frames)
        {
            const std::uint32_t bytesMostra = (std::uint32_t) bits / 8;
            const std::uint32_t dades = (std::uint32_t) std::min<long long> (frames * canals * bytesMostra, 0xffffffffLL - 36);
            std::memcpy (h, "RIFF", 4);
            posaU32 (h + 4, 36 + dades);
            std::memcpy (h + 8, "WAVEfmt ", 8);
            posaU32 (h + 16, 16);
            posaU16 (h + 20, 1);   // PCM
            posaU16 (h + 22, (std::uint16_t) canals);
            posaU32 (h + 24, (std::uint32_t) std::lround (fs));
            posaU32 (h + 28, (std::uint32_t) std::lround (fs) * (std::uint32_t) canals * bytesMostra);
            posaU16 (h + 32, (std::uint16_t) (canals * (int) bytesMostra));
            posaU16 (h + 34, (std::uint16_t) bits);
            std::memcpy (h + 36, "data", 4);
            posaU32 (h + 40, dades);
        }
    }

    bool EscriptorWav::obre (const std::filesystem::path& cami, int nCanals, double freqMostreig, int nBits)
    {
        tanca();
        canals = std::clamp (nCanals, 1, 8);
        bits = nBits == 16 ? 16 : 24;
        fs = freqMostreig > 0.0 ? freqMostreig : 48000.0;
        frames = 0;
        error = false;
        fitxer.open (cami, std::ios::binary | std::ios::trunc);
        if (! fitxer.is_open())
            return false;
        char h[44];
        capcalera (h, canals, fs, bits, 0);
        fitxer.write (h, 44);
        return (bool) fitxer;
    }

    bool EscriptorWav::escriu (const float* d, long long n)
    {
        if (! fitxer.is_open() || n <= 0)
            return fitxer.is_open();
        const int bytes = bits / 8;
        tmp.resize ((size_t) (n * canals * bytes));
        char* o = tmp.data();
        for (long long i = 0; i < n * canals; ++i)
        {
            const float v = std::isfinite (d[i]) ? std::clamp (d[i], -1.0f, 1.0f) : 0.0f;
            if (bits == 16)
            {
                const auto x = (std::int32_t) std::lround (v * 32767.0f);
                o[0] = (char) (x & 0xff); o[1] = (char) ((x >> 8) & 0xff);
            }
            else
            {
                const auto x = (std::int32_t) std::lround ((double) v * 8388607.0);
                o[0] = (char) (x & 0xff); o[1] = (char) ((x >> 8) & 0xff); o[2] = (char) ((x >> 16) & 0xff);
            }
            o += bytes;
        }
        fitxer.write (tmp.data(), (std::streamsize) tmp.size());
        if (! fitxer) error = true;
        frames += n;
        return ! error;
    }

    bool EscriptorWav::tanca()
    {
        if (! fitxer.is_open())
            return ! error;
        // Si el nombre de bytes és senar, cal un byte de farciment.
        if (((frames * canals * (bits / 8)) & 1) != 0)
            fitxer.put (0);
        char h[44];
        capcalera (h, canals, fs, bits, frames);
        fitxer.seekp (0);
        fitxer.write (h, 44);
        if (! fitxer) error = true;
        fitxer.close();
        return ! error;
    }

    WavLlegit llegeixWav (const std::filesystem::path& cami)
    {
        WavLlegit r;
        std::ifstream f (cami, std::ios::binary);
        if (! f) return r;
        std::vector<unsigned char> d ((std::istreambuf_iterator<char> (f)), std::istreambuf_iterator<char>());
        if (d.size() < 12 || std::memcmp (d.data(), "RIFF", 4) != 0 || std::memcmp (d.data() + 8, "WAVE", 4) != 0)
            return r;

        int format = 0, bits = 0;
        size_t p = 12;
        while (p + 8 <= d.size())
        {
            const auto mida = (size_t) llegeixU32 (d.data() + p + 4);
            const auto* cos = d.data() + p + 8;
            if (std::memcmp (d.data() + p, "fmt ", 4) == 0 && mida >= 16)
            {
                format = llegeixU16 (cos);
                r.canals = llegeixU16 (cos + 2);
                r.fs = (double) llegeixU32 (cos + 4);
                bits = llegeixU16 (cos + 14);
                if (format == 0xfffe && mida >= 26) format = llegeixU16 (cos + 24);
            }
            else if (std::memcmp (d.data() + p, "data", 4) == 0 && r.canals > 0)
            {
                const size_t disponible = std::min (mida, d.size() - (p + 8));
                const int bytes = bits / 8;
                if (bytes <= 0) return r;
                const size_t n = disponible / (size_t) bytes;
                r.mostres.resize (n);
                for (size_t i = 0; i < n; ++i)
                {
                    const auto* m = cos + i * (size_t) bytes;
                    if (format == 3 && bits == 32) { float v; std::memcpy (&v, m, 4); r.mostres[i] = v; }
                    else if (bits == 16) r.mostres[i] = (float) (std::int16_t) llegeixU16 (m) / 32768.0f;
                    else if (bits == 24)
                    {
                        std::int32_t x = (std::int32_t) (m[0] | (m[1] << 8) | (m[2] << 16));
                        if (x & 0x800000) x -= 0x1000000;
                        r.mostres[i] = (float) x / 8388608.0f;
                    }
                }
                r.correcte = (format == 1 || format == 3);
                return r;
            }
            p += 8 + mida + (mida & 1);
        }
        return r;
    }
}
