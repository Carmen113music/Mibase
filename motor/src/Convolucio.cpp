#include "mibase/Convolucio.h"
#include "mibase/ProjecteDisc.h"
#include "mibase/Reverb.h"
#include "mibase/Wav.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstring>
#include <thread>

namespace mibase::conv
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        struct C { float r = 0.0f, i = 0.0f; };

        /** FFT complexa de base 2 (iterativa, amb taules fetes al principi). */
        class FFT
        {
        public:
            explicit FFT (int mida) : n (mida), tw ((size_t) mida / 2), rev ((size_t) mida)
            {
                int bits = 0;
                while ((1 << bits) < n) ++bits;
                for (int i = 0; i < n; ++i)
                {
                    int r = 0;
                    for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
                    rev[(size_t) i] = r;
                }
                for (int k = 0; k < n / 2; ++k)
                    tw[(size_t) k] = { (float) std::cos (-2.0 * pi * k / n), (float) std::sin (-2.0 * pi * k / n) };
            }
            void fa (C* x, bool inversa) const noexcept
            {
                for (int i = 0; i < n; ++i)
                    if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);
                for (int len = 2; len <= n; len <<= 1)
                {
                    const int mig = len / 2, pas = n / len;
                    for (int i = 0; i < n; i += len)
                        for (int j = 0; j < mig; ++j)
                        {
                            const C w = tw[(size_t) (j * pas)];
                            const float wi = inversa ? -w.i : w.i;
                            C& a = x[i + j];
                            C& b = x[i + j + mig];
                            const C v { b.r * w.r - b.i * wi, b.r * wi + b.i * w.r };
                            b = { a.r - v.r, a.i - v.i };
                            a = { a.r + v.r, a.i + v.i };
                        }
                }
            }
            int mida() const noexcept { return n; }
        private:
            int n;
            std::vector<C> tw;
            std::vector<int> rev;
        };

        const FFT& fftDe (int n)
        {
            static const FFT a (128), b (4096);
            return n == 128 ? a : b;
        }

        // Filtre de dos pols (per a les pantalles de demostració).
        struct Bq
        {
            double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            double operator() (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
            static Bq rbj (int tipus, double f, double q, double db, double fs)
            {
                const double A = std::pow (10.0, db / 40.0), w = 2.0 * pi * f / fs, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
                double b0, b1, b2, a0, a1, a2;
                if (tipus == 0) { b0 = (1 - cs) / 2; b1 = 1 - cs; b2 = b0; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }              // passa-baix
                else if (tipus == 1) { b0 = (1 + cs) / 2; b1 = -(1 + cs); b2 = b0; a0 = 1 + al; a1 = -2 * cs; a2 = 1 - al; }       // passa-alt
                else { b0 = 1 + al * A; b1 = -2 * cs; b2 = 1 - al * A; a0 = 1 + al / A; a1 = -2 * cs; a2 = 1 - al / A; }          // pic
                Bq r;
                r.b0 = b0 / a0; r.b1 = b1 / a0; r.b2 = b2 / a0; r.a1 = a1 / a0; r.a2 = a2 / a0;
                return r;
            }
        };
    }

    // =========================================================================
    // Motor: la IR ja preparada (no canvia) i l'estat de la convolució
    // =========================================================================
    class Motor
    {
    public:
        static constexpr int PA = 64, LA = 4096, PB = 2048;

        Motor (const std::vector<std::vector<float>>& h, double fs_) : fs (fs_)
        {
            const int nh = (int) h.size();
            if (nh == 1) camins = { { 0, 0, 0 }, { 1, 1, 0 } };
            else if (nh == 2) camins = { { 0, 0, 0 }, { 1, 1, 1 } };
            else if (nh >= 4) camins = { { 0, 0, 0 }, { 0, 1, 1 }, { 1, 0, 2 }, { 1, 1, 3 } };
            numH = std::min (nh, 4);
            for (int k = 0; k < numH; ++k) longitud = std::max (longitud, (long long) h[(size_t) k].size());
            ringL.assign (8192, 0.0f);
            ringR.assign (8192, 0.0f);
            if (longitud <= 0) return;
            const long long lA = std::min<long long> (longitud, LA);
            fesEtapa (A, PA, (int) ((lA + PA - 1) / PA), 0, h);
            if (longitud > LA) fesEtapa (B, PB, (int) ((longitud - LA + PB - 1) / PB), LA, h);
            for (auto& v : nouB) v.assign ((size_t) PB, 0.0f);
        }

        long long longitud = 0;
        double fs = 48000.0;

        void buida() noexcept
        {
            for (Etapa* e : { &A, &B })
            {
                std::fill (e->X.begin(), e->X.end(), C {});
                for (auto& v : e->in) std::fill (v.begin(), v.end(), 0.0f);
                for (auto& v : e->acc) std::fill (v.begin(), v.end(), C {});
                e->cap = 0;
            }
            std::fill (ringL.begin(), ringL.end(), 0.0f);
            std::fill (ringR.begin(), ringR.end(), 0.0f);
            for (auto& v : nouB) std::fill (v.begin(), v.end(), 0.0f);
            pos = 0; nB = 0; feina = false; fetes = total = desDe = 0;
        }

        inline void mostra (float iL, float iR, float& oL, float& oR) noexcept
        {
            tros[0][pos] = iL;
            tros[1][pos] = iR;
            const size_t k = (size_t) (r & 8191);
            oL = ringL[k];
            oR = ringR[k];
            ringL[k] = ringR[k] = 0.0f;
            ++r;
            if (++pos == PA)
            {
                pos = 0;
                if (longitud > 0) trosComplet();
            }
        }

    private:
        struct Cami { int in, out, h; };
        struct Etapa
        {
            int P = 0, N = 0, parts = 0;
            long long offset = 0;
            std::vector<C> H, X, tmp;
            std::vector<float> in[2];
            std::vector<C> acc[2];
            int cap = 0;
        };

        void fesEtapa (Etapa& e, int P, int parts, long long offset, const std::vector<std::vector<float>>& h)
        {
            e.P = P; e.N = 2 * P; e.parts = std::max (1, parts); e.offset = offset;
            const int nb = P + 1;
            e.H.assign ((size_t) numH * (size_t) e.parts * (size_t) nb, C {});
            e.X.assign ((size_t) 2 * (size_t) e.parts * (size_t) nb, C {});
            e.tmp.assign ((size_t) e.N, C {});
            for (auto& v : e.in) v.assign ((size_t) e.N, 0.0f);
            for (auto& v : e.acc) v.assign ((size_t) nb, C {});
            const auto& f = fftDe (e.N);
            for (int k = 0; k < numH; ++k)
                for (int j = 0; j < e.parts; ++j)
                {
                    std::fill (e.tmp.begin(), e.tmp.end(), C {});
                    const auto& v = h[(size_t) k];
                    for (int i = 0; i < P; ++i)
                    {
                        const long long q = offset + (long long) j * P + i;
                        if (q < (long long) v.size()) e.tmp[(size_t) i].r = v[(size_t) q];
                    }
                    f.fa (e.tmp.data(), false);
                    std::copy (e.tmp.begin(), e.tmp.begin() + nb, e.H.begin() + ((std::ptrdiff_t) k * e.parts + j) * nb);
                }
        }

        void fftEntrada (Etapa& e, int ch) noexcept
        {
            const int nb = e.P + 1;
            for (int i = 0; i < e.N; ++i) e.tmp[(size_t) i] = { e.in[ch][(size_t) i], 0.0f };
            fftDe (e.N).fa (e.tmp.data(), false);
            std::copy (e.tmp.begin(), e.tmp.begin() + nb, e.X.begin() + ((std::ptrdiff_t) ch * e.parts + e.cap) * nb);
        }

        void mac (Etapa& e, int j) noexcept
        {
            const int nb = e.P + 1;
            const int slot = (e.cap - j + e.parts) % e.parts;
            for (const auto& c : camins)
            {
                const C* x = e.X.data() + ((std::ptrdiff_t) c.in * e.parts + slot) * nb;
                const C* hh = e.H.data() + ((std::ptrdiff_t) c.h * e.parts + j) * nb;
                C* a = e.acc[c.out].data();
                for (int k = 0; k < nb; ++k)
                {
                    a[k].r += x[k].r * hh[k].r - x[k].i * hh[k].i;
                    a[k].i += x[k].r * hh[k].i + x[k].i * hh[k].r;
                }
            }
        }

        void ifftSortida (Etapa& e, int out, long long on) noexcept
        {
            const int P = e.P, N = e.N;
            auto& a = e.acc[out];
            e.tmp[0] = a[0];
            e.tmp[(size_t) P] = a[(size_t) P];
            for (int k = 1; k < P; ++k)
            {
                e.tmp[(size_t) k] = a[(size_t) k];
                e.tmp[(size_t) (N - k)] = { a[(size_t) k].r, -a[(size_t) k].i };
            }
            fftDe (N).fa (e.tmp.data(), true);
            const float esc = 1.0f / (float) N;
            float* ring = out == 0 ? ringL.data() : ringR.data();
            for (int k = 0; k < P; ++k) ring[(size_t) ((on + k) & 8191)] += e.tmp[(size_t) (P + k)].r * esc;
            std::fill (a.begin(), a.end(), C {});
        }

        void trosComplet() noexcept
        {
            // ---- Etapa A (cada 64 mostres, al moment) ----
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& v = A.in[ch];
                std::memmove (v.data(), v.data() + PA, sizeof (float) * PA);
                std::memcpy (v.data() + PA, tros[ch], sizeof (float) * PA);
            }
            A.cap = (A.cap + 1) % A.parts;
            fftEntrada (A, 0);
            fftEntrada (A, 1);
            for (int j = 0; j < A.parts; ++j) mac (A, j);
            ifftSortida (A, 0, r);
            ifftSortida (A, 1, r);

            // ---- Etapa B (cada 2048 mostres; la feina es reparteix entre els 32 trossos següents) ----
            if (B.parts == 0 || B.N == 0) return;
            for (int ch = 0; ch < 2; ++ch) std::memcpy (nouB[ch].data() + nB, tros[ch], sizeof (float) * PA);
            nB += PA;
            if (nB == PB)
            {
                while (feina && fetes < total) unitat();
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& v = B.in[ch];
                    std::memmove (v.data(), v.data() + PB, sizeof (float) * PB);
                    std::memcpy (v.data() + PB, nouB[ch].data(), sizeof (float) * PB);
                }
                nB = 0;
                feina = true;
                fetes = 0;
                total = 4 + B.parts;
                desDe = 0;
                r0 = r;
            }
            if (feina)
            {
                ++desDe;
                const int obj = std::min (total, (total * desDe + 31) / 32);
                while (fetes < obj) unitat();
                if (fetes >= total) feina = false;
            }
        }

        void unitat() noexcept
        {
            const int u = fetes++;
            if (u == 0) { B.cap = (B.cap + 1) % B.parts; fftEntrada (B, 0); }
            else if (u == 1) fftEntrada (B, 1);
            else if (u < 2 + B.parts) mac (B, u - 2);
            else ifftSortida (B, u - 2 - B.parts, r0 + PB + PA);
        }

        std::vector<Cami> camins;
        int numH = 0;
        Etapa A, B;
        float tros[2][PA] {};
        int pos = 0;
        std::vector<float> nouB[2];
        int nB = 0;
        bool feina = false;
        int fetes = 0, total = 0, desDe = 0;
        long long r0 = 0;
        std::vector<float> ringL, ringR;
        long long r = 0;
    };

    // =========================================================================
    // Treballador: el fil de fons que prepara els motors nous
    // =========================================================================
    class Treballador
    {
    public:
        static Treballador& get()
        {
            static Treballador* t = new Treballador();   // no es destrueix mai (el fil viu fins al final del programa)
            return *t;
        }
        void registra (Convolucio* c) { std::lock_guard<std::mutex> l (m); v.push_back ({ c, 0 }); }
        void treu (Convolucio* c)
        {
            std::lock_guard<std::mutex> l (m);
            v.erase (std::remove_if (v.begin(), v.end(), [c] (const Reg& x) { return x.c == c; }), v.end());
        }

    private:
        struct Reg { Convolucio* c; unsigned vista; };
        Treballador()
        {
            std::thread ([this]
            {
                for (;;)
                {
                    std::this_thread::sleep_for (std::chrono::milliseconds (30));
                    std::lock_guard<std::mutex> l (m);
                    for (auto& x : v)
                    {
                        if (auto* e = x.c->perEsborrar.exchange (nullptr)) delete e;
                        // Espera que la versió no canviï durant un tomb (mentre es mou un pom, no es fa a cada pas).
                        const unsigned ver = x.c->versio.load();
                        if (ver == x.c->versioFeta.load()) continue;
                        if (ver != x.vista) { x.vista = ver; continue; }
                        x.c->feina();
                    }
                }
            }).detach();
        }
        std::mutex m;
        std::vector<Reg> v;
    };

    // =========================================================================
    // Convolucio
    // =========================================================================
    Convolucio::Convolucio() { Treballador::get().registra (this); }

    Convolucio::~Convolucio()
    {
        Treballador::get().treu (this);
        delete actiu;
        delete vell;
        delete pendent.exchange (nullptr);
        delete perEsborrar.exchange (nullptr);
    }

    void Convolucio::posaIR (std::shared_ptr<const IR> ir)
    {
        {
            std::lock_guard<std::mutex> l (mtx);
            irActual = std::move (ir);
        }
        versio.fetch_add (1);
    }

    std::shared_ptr<const IR> Convolucio::ir() const
    {
        std::lock_guard<std::mutex> l (mtx);
        return irActual;
    }

    void Convolucio::posaOpcions (const Opcions& o) noexcept
    {
        const Opcions ara = opcions();
        if (ara == o) return;
        oLong.store ((float) std::clamp (o.longitud, 0.05, 1.0));
        oDamp.store ((float) std::clamp (o.damping, 0.0, 1.0));
        oNorm.store (o.normalitza);
        oRev.store (o.reversa);
        versio.fetch_add (1);
    }

    Opcions Convolucio::opcions() const noexcept
    {
        Opcions o;
        o.longitud = oLong.load();
        o.damping = oDamp.load();
        o.normalitza = oNorm.load();
        o.reversa = oRev.load();
        return o;
    }

    bool Convolucio::feina (bool forcar)
    {
        std::lock_guard<std::mutex> l (mtx);
        const unsigned ver = versio.load();
        if ((! forcar && ver == versioFeta.load()) || fs <= 0.0) return false;
        versioFeta.store (ver);
        Motor* m = irActual != nullptr && irActual->frames() > 0 ? new Motor (transforma (*irActual, opcions(), fs), fs)
                                                                 : new Motor ({}, fs);
        delete pendent.exchange (m);
        return true;
    }

    void Convolucio::prepara (double nouFs)
    {
        std::lock_guard<std::mutex> l (mtx);
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        versioFeta.store (versio.load());
        delete actiu;
        delete vell;
        delete pendent.exchange (nullptr);
        actiu = irActual != nullptr && irActual->frames() > 0 ? new Motor (transforma (*irActual, opcions(), fs), fs) : nullptr;
        vell = nullptr;
        fosa = 1.0f;
        longActiva.store (actiu != nullptr ? actiu->longitud : 0);
    }

    void Convolucio::espera() { feina (false); }

    void Convolucio::canvia() noexcept
    {
        Motor* nou = pendent.exchange (nullptr, std::memory_order_acq_rel);
        if (nou == nullptr) return;
        // (un motor sense IR també serveix: no fa so, i així el que sonava s'apaga amb la fosa)
        if (actiu != nullptr) { vell = actiu; fosa = 0.0f; }
        actiu = nou;
        longActiva.store (nou->longitud, std::memory_order_relaxed);
    }

    void Convolucio::mostra (float iL, float iR, float& oL, float& oR) noexcept
    {
        if (demanaBuidar.exchange (false, std::memory_order_acq_rel))
        {
            if (actiu != nullptr) actiu->buida();
            if (vell != nullptr) vell->buida();
        }
        if ((compta++ & 63) == 0 && vell == nullptr) canvia();
        oL = oR = 0.0f;
        if (actiu != nullptr) actiu->mostra (iL, iR, oL, oR);
        if (vell != nullptr)
        {
            float vL = 0.0f, vR = 0.0f;
            vell->mostra (iL, iR, vL, vR);
            fosa = std::min (1.0f, fosa + 1.0f / (0.25f * (float) vell->fs));
            const float gNou = std::sqrt (fosa), gVell = std::sqrt (1.0f - fosa);
            oL = oL * gNou + vL * gVell;
            oR = oR * gNou + vR * gVell;
            if (fosa >= 1.0f && perEsborrar.load (std::memory_order_acquire) == nullptr)
            {
                perEsborrar.store (vell, std::memory_order_release);
                vell = nullptr;
            }
        }
    }

    // =========================================================================
    // Fitxers i transformacions
    // =========================================================================
    std::shared_ptr<IR> carrega (const std::filesystem::path& cami, std::string& error)
    {
        std::error_code ec;
        if (! std::filesystem::exists (cami, ec)) { error = "No es troba el fitxer"; return nullptr; }
        const auto w = llegeixWav (cami);
        if (! w.correcte || w.canals <= 0 || w.frames() <= 0) { error = "No és un WAV que Mibase sàpiga llegir"; return nullptr; }
        auto ir = std::make_shared<IR>();
        ir->fs = w.fs > 0.0 ? w.fs : 48000.0;
        const int nc = w.canals >= 4 ? 4 : std::min (w.canals, 2);
        const long long fr = std::min<long long> (w.frames(), (long long) (duradaMaxima * ir->fs));
        ir->c.assign ((size_t) nc, std::vector<float> ((size_t) fr, 0.0f));
        for (long long i = 0; i < fr; ++i)
            for (int k = 0; k < nc; ++k)
                ir->c[(size_t) k][(size_t) i] = w.mostres[(size_t) (i * w.canals + k)];
        ir->nom = disc::text (cami.stem());
        ir->referencia = referencia (cami);
        return ir;
    }

    static std::vector<float> remostreja (const std::vector<float>& x, double de, double a)
    {
        if (std::abs (de - a) < 0.5 || x.empty()) return x;
        const double ratio = a / de;   // mostres noves per cada vella
        const long long n = (long long) std::ceil ((double) x.size() * ratio - 1.0e-6);
        std::vector<float> y ((size_t) n, 0.0f);
        const double tall = std::min (1.0, ratio);   // passa-baix quan es baixa la freqüència
        const int ample = 16;
        const double radi = ample / tall;
        for (long long i = 0; i < n; ++i)
        {
            const double t = (double) i / ratio;
            const long long c = (long long) std::floor (t);
            double s = 0.0;
            for (long long k = c - (long long) radi; k <= c + (long long) radi + 1; ++k)
            {
                if (k < 0 || k >= (long long) x.size()) continue;
                const double d = t - (double) k;
                const double u = d * tall;
                const double sinc = std::abs (u) < 1.0e-9 ? 1.0 : std::sin (pi * u) / (pi * u);
                const double wv = std::abs (d) >= radi ? 0.0 : 0.42 + 0.5 * std::cos (pi * d / radi) + 0.08 * std::cos (2.0 * pi * d / radi);
                s += x[(size_t) k] * sinc * wv * tall;
            }
            y[(size_t) i] = (float) s;
        }
        return y;
    }

    std::vector<std::vector<float>> transforma (const IR& ir, const Opcions& o, double fs)
    {
        std::vector<std::vector<float>> h;
        if (ir.c.empty()) return h;
        const int nc = ir.canals() >= 4 ? 4 : std::min (ir.canals(), 2);
        for (int k = 0; k < nc; ++k)
        {
            auto v = remostreja (ir.c[(size_t) k], ir.fs, fs);
            if (! o.normalitza && std::abs (ir.fs - fs) >= 0.5)
                for (auto& x : v) x *= (float) (ir.fs / fs);
            h.push_back (std::move (v));
        }
        long long L = 0;
        for (auto& v : h) L = std::max (L, (long long) v.size());
        L = std::min (L, (long long) (duradaMaxima * fs));
        const long long nova = std::max<long long> (64, (long long) std::llround ((double) L * std::clamp (o.longitud, 0.05, 1.0)));
        for (auto& v : h)
        {
            v.resize ((size_t) nova, 0.0f);
            // Damping: un passa-baix que es va tancant amb el temps.
            if (o.damping > 0.001)
            {
                double y = 0.0;
                for (long long i = 0; i < nova; ++i)
                {
                    const double t = (double) i / fs;
                    const double fc = std::max (180.0, 20000.0 * std::pow (2.0, -o.damping * 7.0 * t));
                    const double a = std::exp (-2.0 * pi * std::min (fc, fs * 0.45) / fs);
                    y = (1.0 - a) * v[(size_t) i] + a * y;
                    v[(size_t) i] = (float) y;
                }
            }
            // Baixada suau al final (si s'ha escurçat, més llarga).
            const long long fade = o.longitud < 0.999 ? std::max<long long> (64, nova * 15 / 100) : std::min<long long> (64, nova / 4);
            for (long long i = 0; i < fade; ++i)
            {
                const double g = 0.5 - 0.5 * std::cos (pi * (double) i / (double) fade);
                v[(size_t) (nova - 1 - i)] *= (float) g;
            }
            if (o.reversa) std::reverse (v.begin(), v.end());
        }
        if (o.normalitza)
        {
            double e = 0.0;
            for (auto& v : h) for (float x : v) e += (double) x * x;
            // energia que arriba a cada sortida (mono: la mateixa IR a totes dues; true stereo: dos camins per sortida)
            const double perSortida = nc == 1 ? e : (nc == 2 ? e / 2.0 : e / 2.0);
            if (perSortida > 1.0e-12)
            {
                const float g = (float) (0.35 / std::sqrt (perSortida));
                for (auto& v : h) for (auto& x : v) x *= g;
            }
        }
        return h;
    }

    // =========================================================================
    // Biblioteca
    // =========================================================================
    namespace { std::filesystem::path& arrelGlobal() { static std::filesystem::path p; return p; } }

    void posaArrel (const std::filesystem::path& carpeta) { arrelGlobal() = carpeta; }
    std::filesystem::path arrel() { return arrelGlobal(); }

    std::string referencia (const std::filesystem::path& cami)
    {
        const auto& a = arrelGlobal();
        if (! a.empty())
        {
            std::error_code ec;
            const auto ca = std::filesystem::weakly_canonical (a, ec);
            const auto cc = std::filesystem::weakly_canonical (cami, ec);
            const auto rel = cc.lexically_relative (ca);
            auto t = disc::text (rel);
            if (! rel.empty() && ! rel.is_absolute() && t.rfind ("..", 0) != 0)
            {
                for (auto& c : t) if (c == '\\') c = '/';
                return "bib:" + t;
            }
        }
        return disc::text (cami);
    }

    std::filesystem::path resol (const std::string& ref)
    {
        if (ref.rfind ("bib:", 0) == 0) return arrelGlobal() / disc::cami (ref.substr (4));
        return disc::cami (ref);
    }

    std::vector<EntradaBiblioteca> llistaBiblioteca()
    {
        std::vector<EntradaBiblioteca> v;
        const auto a = arrelGlobal();
        std::error_code ec;
        if (a.empty() || ! std::filesystem::is_directory (a, ec)) return v;
        for (auto it = std::filesystem::recursive_directory_iterator (a, ec); it != std::filesystem::recursive_directory_iterator(); it.increment (ec))
        {
            if (ec) break;
            if (! it->is_regular_file (ec)) continue;
            auto ext = disc::text (it->path().extension());
            for (auto& c : ext) c = (char) std::tolower ((unsigned char) c);
            if (ext != ".wav") continue;
            const auto rel = it->path().lexically_relative (a);
            EntradaBiblioteca e;
            e.categoria = std::distance (rel.begin(), rel.end()) > 1 ? disc::text (*rel.begin()) : std::string ("Altres");
            e.nom = disc::text (it->path().stem());
            e.cami = it->path();
            v.push_back (std::move (e));
        }
        std::sort (v.begin(), v.end(), [] (const EntradaBiblioteca& x, const EntradaBiblioteca& y)
                   { return x.categoria != y.categoria ? x.categoria < y.categoria : x.nom < y.nom; });
        return v;
    }

    const std::vector<std::string>& categoriesDemo()
    {
        static const std::vector<std::string> c { "Rooms", "Studios", "Halls", "Churches", "Plates", "Springs", "Special FX", "Cabinets" };
        return c;
    }

    int creaBibliotecaDemo (const std::filesystem::path& carpeta, double fs)
    {
        using namespace mibase::reverb;
        struct Demo { const char* cat; const char* nom; int alg; float decay, size, pre, lowCut, highCut, width; };
        const Demo demos[] = {
            { "Rooms", "Habitació petita", Room, 0.45f, 30, 0, 80, 12000, 80 },
            { "Rooms", "Habitació de fusta", Room, 0.8f, 50, 4, 100, 8000, 90 },
            { "Studios", "Estudi sec", Room, 0.3f, 22, 0, 120, 14000, 70 },
            { "Studios", "Sala d'estudi", Chamber, 1.0f, 45, 6, 90, 11000, 100 },
            { "Halls", "Sala mitjana", Hall, 1.8f, 60, 12, 60, 10000, 100 },
            { "Halls", "Sala de concerts", Hall, 2.8f, 85, 22, 50, 9000, 110 },
            { "Churches", "Església de pedra", Hall, 4.5f, 95, 30, 70, 7500, 110 },
            { "Churches", "Catedral", Hall, 7.0f, 100, 45, 60, 6500, 120 },
            { "Plates", "Placa brillant", Plate, 1.6f, 55, 0, 150, 16000, 100 },
            { "Plates", "Placa fosca", Plate, 2.4f, 65, 8, 120, 6000, 100 },
            { "Springs", "Moll curt", Spring, 1.2f, 40, 0, 200, 6000, 80 },
            { "Springs", "Moll de guitarra", Spring, 2.2f, 60, 0, 150, 5000, 90 },
            { "Special FX", "Revers", Reverse, 1.5f, 60, 0, 100, 12000, 100 },
            { "Special FX", "Gated 80", Gated, 1.0f, 60, 0, 120, 11000, 100 },
            { "Special FX", "Shimmer", Shimmer, 4.0f, 90, 20, 200, 12000, 120 },
        };
        int fets = 0;
        std::error_code ec;
        auto escriu = [&] (const std::filesystem::path& f, const std::vector<float>& L, const std::vector<float>& R)
        {
            float pic = 1.0e-9f;
            for (size_t i = 0; i < L.size(); ++i) pic = std::max ({ pic, std::abs (L[i]), std::abs (R[i]) });
            std::vector<float> s (L.size() * 2);
            for (size_t i = 0; i < L.size(); ++i) { s[2 * i] = L[i] * 0.9f / pic; s[2 * i + 1] = R[i] * 0.9f / pic; }
            std::filesystem::create_directories (f.parent_path(), ec);
            EscriptorWav w;
            if (w.obre (f, 2, fs, 24) && w.escriu (s.data(), (long long) L.size()) && w.tanca()) ++fets;
        };
        for (const auto& d : demos)
        {
            const auto f = carpeta / d.cat / disc::cami (std::string (d.nom) + ".wav");
            if (std::filesystem::exists (f, ec)) continue;
            Params p;
            p.posa (Alg, (float) d.alg); p.posa (Mix, 100); p.posa (Decay, d.decay); p.posa (Size, d.size); p.posa (PreDelay, d.pre);
            p.posa (LowCut, d.lowCut); p.posa (HighCut, d.highCut); p.posa (Width, d.width); p.posa (ModDepth, 0); p.posa (Sortida, 0);
            p.posa (Qualitat, 2);
            Reverb r;
            r.posaParams (p);
            r.prepara (fs, 512);
            const long long n = (long long) ((d.decay * 1.25 + d.pre * 0.001 + 0.2) * fs);
            std::vector<float> L ((size_t) n, 0.0f), R ((size_t) n, 0.0f);
            L[0] = R[0] = 1.0f;
            for (long long i = 0; i < n; i += 512) r.processa (L.data() + i, R.data() + i, (int) std::min<long long> (512, n - i));
            escriu (f, L, R);
        }
        // Pantalles de guitarra (cabinets): respostes curtes fetes amb filtres.
        struct Pantalla { const char* nom; double hp, ressF, ressDb, presF, presDb, lp; };
        const Pantalla pant[] = { { "Pantalla 1x12 oberta", 75, 110, 3, 2200, 4, 5200 }, { "Pantalla 4x12 tancada", 85, 95, 5, 1800, 6, 4300 } };
        for (const auto& pd : pant)
        {
            const auto f = carpeta / "Cabinets" / disc::cami (std::string (pd.nom) + ".wav");
            if (std::filesystem::exists (f, ec)) continue;
            const int n = (int) (0.08 * fs);
            std::vector<float> L ((size_t) n), R ((size_t) n);
            for (int c = 0; c < 2; ++c)
            {
                Bq f1 = Bq::rbj (1, pd.hp, 0.8, 0, fs), f2 = Bq::rbj (2, pd.ressF, 1.2, pd.ressDb, fs), f3 = Bq::rbj (2, pd.presF * (c ? 1.06 : 1.0), 1.0, pd.presDb, fs),
                   f4 = Bq::rbj (0, pd.lp, 0.7, 0, fs), f5 = Bq::rbj (0, pd.lp * 1.15, 0.6, 0, fs), f6 = Bq::rbj (2, 3800, 4.0, -6, fs);
                auto& v = c ? R : L;
                for (int i = 0; i < n; ++i)
                {
                    double x = i == 0 ? 1.0 : 0.0;
                    if (i == (int) (0.0007 * fs)) x += 0.25;   // la reflexió del cos de la caixa
                    v[(size_t) i] = (float) f6 (f5 (f4 (f3 (f2 (f1 (x))))));
                }
            }
            escriu (f, L, R);
        }
        return fets;
    }
}
