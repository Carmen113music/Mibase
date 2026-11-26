#include "mibase/Diagnosi.h"
#include "mibase/Dinamica.h"
#include "mibase/Equalitzador.h"
#include "mibase/Estereo.h"
#include "mibase/FFT.h"
#include "mibase/LowEnd.h"
#include "mibase/Rack.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>

namespace mibase::diagnosi
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        // zones (índexs de banda, inclosos)
        struct Zona { int a, b; const char* nom; };
        constexpr Zona sub { 0, 2, "subgreus" }, greus { 3, 6, "greus" }, fang { 8, 11, "greus-mitjans" }, mitjos { 12, 16, "mitjos" },
                       presencia { 17, 21, "presència" }, brillantor { 22, 24, "brillantor" }, aire { 25, 27, "aire" };

        std::string num (float v, int d = 1)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", d, (double) v);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }
        std::string hz (float f)
        {
            if (f >= 1000.0f) return num (f / 1000.0f, f >= 10000.0f ? 0 : 1) + " kHz";
            return std::to_string ((int) std::lround (f)) + " Hz";
        }
        std::string signe (float v, int d = 1) { return (v > 0 ? "+" : "") + num (v, d); }
        float clampf (float x, float a, float b) { return std::max (a, std::min (b, x)); }
    }

    float freqBanda (int b) { return 31.5f * std::pow (2.0f, (float) b / 3.0f); }

    std::array<float, numBandes> Analisi::corba() const
    {
        std::array<float, numBandes> c {};
        for (int b = 0; b < numBandes; ++b) c[(size_t) b] = (float) (10.0 * std::log10 (mid[(size_t) b] + side[(size_t) b] + 1.0e-20));
        float ref = 0.0f;
        for (int b = 12; b <= 18; ++b) ref += c[(size_t) b];
        ref /= 7.0f;
        for (auto& x : c) x -= ref;
        return c;
    }

    double Analisi::energia (int b0, int b1) const
    {
        double e = 0.0;
        for (int b = b0; b <= b1; ++b) e += mid[(size_t) b] + side[(size_t) b];
        return e;
    }

    namespace
    {
        float referenciaCrua (int b);
    }

    float referencia (int b)
    {
        // 1.12: relativa a la mitjana de 500 Hz–2 kHz, igual que la corba del mix (abans era relativa a 1 kHz i hi havia 0,4 dB de biaix)
        static const float mitjana = []
        {
            float m = 0.0f;
            for (int k = 12; k <= 18; ++k) m += referenciaCrua (k);
            return m / 7.0f;
        }();
        return referenciaCrua (b) - mitjana;
    }

    namespace
    {
    float referenciaCrua (int b)
    {
        // forma mitjana orientativa d'un pop/rock d'avui (terços d'octava, relativa a 500 Hz–2 kHz)
        static const float pf[] = { 31.5f, 50.0f, 100.0f, 200.0f, 400.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 12500.0f, 16000.0f };
        static const float pd[] = { 1.0f, 5.0f, 5.5f, 3.5f, 1.5f, 0.0f, -2.5f, -5.0f, -8.5f, -12.0f, -16.0f };
        constexpr int n = 11;
        const float f = freqBanda (b);
        if (f <= pf[0]) return pd[0];
        for (int k = 1; k < n; ++k)
            if (f <= pf[k])
            {
                const float t = std::log2 (f / pf[k - 1]) / std::log2 (pf[k] / pf[k - 1]);
                return pd[k - 1] + t * (pd[k] - pd[k - 1]);
            }
        return pd[n - 1];
    }
    }

    Analisi analitza (const float* L, const float* R, std::size_t n, double fs)
    {
        Analisi a;
        if (L == nullptr || R == nullptr || n < 4096 || fs < 8000.0) return a;
        a.segons = (double) n / fs;
        constexpr int N = 8192, hop = N / 2;
        fft::FFT f (N);
        std::vector<fft::C> x ((size_t) N);
        std::vector<float> w ((size_t) N);
        for (int i = 0; i < N; ++i) w[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / (N - 1)));
        // a quina banda va cada bin (−1: fora)
        std::vector<int> banda ((size_t) N / 2, -1);
        for (int k = 1; k < N / 2; ++k)
        {
            const double fk = (double) k * fs / N;
            const int b = (int) std::lround (3.0 * std::log2 (fk / 31.5));
            if (b >= 0 && b < numBandes && fk >= 28.0 && fk <= 18000.0) banda[(size_t) k] = b;
        }
        for (std::size_t o = 0; o + N <= n || (o == 0 && n >= (std::size_t) N); o += hop)
        {
            if (o + N > n) break;
            for (int i = 0; i < N; ++i)
            {
                const float l = L[o + (size_t) i], r = R[o + (size_t) i];
                x[(size_t) i] = { 0.5f * (l + r) * w[(size_t) i], 0.5f * (l - r) * w[(size_t) i] };
            }
            f.fa (x.data(), false);
            for (int k = 1; k < N / 2; ++k)
            {
                const int b = banda[(size_t) k];
                if (b < 0) continue;
                const fft::C X = x[(size_t) k], Y { x[(size_t) (N - k)].r, -x[(size_t) (N - k)].i };   // conj(X[N−k])
                const double mr = 0.5 * (X.r + Y.r), mi = 0.5 * (X.i + Y.i);
                const double sr = 0.5 * (X.i - Y.i), si = -0.5 * (X.r - Y.r);                       // (X − Y) / 2j
                a.mid[(size_t) b] += mr * mr + mi * mi;
                a.side[(size_t) b] += sr * sr + si * si;
            }
        }
        a.loud = loudness::analitza (L, R, n, fs);
        double ll = 0, rr = 0, lr = 0, mm = 0, ss = 0;
        for (std::size_t i = 0; i < n; ++i)
        {
            const double l = L[i], r = R[i];
            ll += l * l; rr += r * r; lr += l * r;
            mm += 0.25 * (l + r) * (l + r); ss += 0.25 * (l - r) * (l - r);
        }
        a.correlacio = ll > 1e-12 && rr > 1e-12 ? (float) (lr / std::sqrt (ll * rr)) : 1.0f;
        a.sideMidDb = mm > 1e-12 ? (float) (10.0 * std::log10 (std::max (ss, 1e-20) / mm)) : -100.0f;
        a.valid = a.loud.integrated > -70.0f;
        return a;
    }

    const char* nomMena (Mena m) { return m == Mena::Problema ? "PROBLEMA" : m == Mena::Oportunitat ? "OPORTUNITAT" : "PREFERÈNCIA"; }
    const char* nomConfianca (Confianca c) { return c == Confianca::Alta ? "alta" : c == Confianca::Mitjana ? "mitjana" : "baixa"; }

    Informe diagnostica (const Analisi& mix, const Analisi* master, const std::vector<std::pair<std::string, Analisi>>& pistes, const Context& ctx)
    {
        Informe inf;
        inf.mix = mix;
        if (master != nullptr) { inf.master = *master; inf.ambMaster = master->valid; }
        for (auto& p : pistes) if (p.second.valid) ++inf.pistesAnalitzades;
        if (! mix.valid)
        {
            inf.confianca = Confianca::Baixa;
            inf.confiancaPerque = "No hi ha prou so per analitzar (la mescla és buida o molt fluixa).";
            inf.enginyer = "Primer cal que la cançó soni: no hi ha res a analitzar.";
            return inf;
        }
        const auto c = mix.corba();
        auto referencia = [&ctx] (int b) { return ctx.ambReferencia ? ctx.corbaRef[(size_t) b] : diagnosi::referencia (b); };   // 1.01
        auto desv = [&] (const Zona& z) { float s = 0; for (int b = z.a; b <= z.b; ++b) s += c[(size_t) b] - referencia (b); return s / (float) (z.b - z.a + 1); };
        auto pic = [&] (const Zona& z, bool maxim)
        {
            int m = z.a;
            for (int b = z.a; b <= z.b; ++b)
            {
                const float d = c[(size_t) b] - referencia (b), dm = c[(size_t) m] - referencia (m);
                if (maxim ? d > dm : d < dm) m = b;
            }
            return freqBanda (m);
        };
        auto culpables = [&] (Troballa& t, const Zona& z, bool nomesSide = false)
        {
            if (inf.pistesAnalitzades < 2) return;
            std::vector<std::pair<double, std::string>> v;
            double tot = 0.0;
            for (auto& p : pistes)
            {
                if (! p.second.valid) continue;
                double e = 0.0;
                for (int b = z.a; b <= z.b; ++b) e += nomesSide ? p.second.side[(size_t) b] : p.second.mid[(size_t) b] + p.second.side[(size_t) b];
                v.push_back ({ e, p.first });
                tot += e;
            }
            if (tot <= 0.0) return;
            std::sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.first > b.first; });
            for (size_t k = 0; k < v.size() && k < 3; ++k)
            {
                const float pc = (float) (100.0 * v[k].first / tot);
                if (pc >= 15.0f) t.culpables.push_back ({ v[k].second, pc });
            }
            if (! t.culpables.empty() && t.culpables.front().percent >= 55.0f)
            {
                t.deMescla = true;
                t.perque += " Sobretot ve de «" + t.culpables.front().pista + "» (" + std::to_string ((int) std::lround (t.culpables.front().percent))
                          + " %): s'arregla millor a la pista (amb l'EQ de la pista) que al màster, que ho tocaria tot.";
            }
        };
        auto conf = [] (float d, float alta) { return std::abs (d) >= alta ? Confianca::Alta : Confianca::Mitjana; };
        std::vector<Troballa>& v = inf.troballes;

        // ---- to ----
        if (const float d = desv (fang); d > 2.5f)
        {
            Troballa t; t.mena = Mena::Problema; t.confianca = conf (d, 4.0f);
            const float f = pic (fang, true), g = -clampf (0.6f * d, 1.0f, 3.5f);
            t.titol = "Acumulació de greus-mitjans (fang)";
            t.on = "Entre 200 i 400 Hz, sobretot al voltant de " + hz (f) + " (" + signe (d) + " dB respecte de la referència).";
            t.perque = "Pot tapar la claredat de les guitarres i la veu i fer que la mescla soni congestionada.";
            t.eina = "EQ de màster";
            t.canvi = num (g) + " dB a " + hz (f) + ", Q ample (0,9). Rang raonable: de -1 a " + num (std::min (-1.0f, g - 1.0f)) + " dB.";
            t.objectiu = "Més claredat sense perdre el cos.";
            t.accio = { Accio::EqCampana, f, g, 0.9f };
            t.gravetat = d + 1.0f;
            culpables (t, fang);
            v.push_back (t);
        }
        if (const float d = desv (greus); d > 3.0f)
        {
            Troballa t; t.mena = Mena::Problema; t.confianca = conf (d, 5.0f);
            const float g = -clampf (0.5f * d, 1.0f, 3.0f);
            t.titol = "Massa greus";
            t.on = "Entre 60 i 125 Hz (" + signe (d) + " dB).";
            t.perque = "Els greus gasten molt volum: el limitador treballarà més i el màster sonarà més petit i bombejant.";
            t.eina = "EQ de màster (prestatge de greus) o Low End Focus";
            t.canvi = num (g) + " dB al prestatge de greus (100 Hz).";
            t.objectiu = "Greus més controlats i més marge per al volum.";
            t.accio = { Accio::EqGreus, 100.0f, g };
            t.gravetat = d + 1.0f;
            culpables (t, greus);
            v.push_back (t);
        }
        else if (d < -3.5f)
        {
            Troballa t; t.mena = Mena::Oportunitat; t.confianca = Confianca::Mitjana;
            const float g = clampf (-0.4f * d, 0.5f, 2.5f);
            t.titol = "Falten greus";
            t.on = "Entre 60 i 125 Hz (" + signe (d) + " dB).";
            t.perque = "La cançó pot sonar prima, sobretot en equips grans.";
            t.eina = "EQ de màster (prestatge de greus)";
            t.canvi = signe (g) + " dB al prestatge de greus (100 Hz).";
            t.objectiu = "Més cos i base.";
            t.accio = { Accio::EqGreus, 100.0f, g };
            t.gravetat = -d;
            v.push_back (t);
        }
        if (const float d = desv (sub); d > 4.0f)
        {
            Troballa t; t.mena = Mena::Problema; t.confianca = Confianca::Mitjana;
            const float g = -clampf (0.5f * d, 1.0f, 3.0f);
            t.titol = "Massa subgreu";
            t.on = "Per sota de 50 Hz (" + signe (d) + " dB).";
            t.perque = "Als altaveus normals no se sent, però gasta volum que el limitador haurà de treure.";
            t.eina = "EQ de màster";
            t.canvi = num (g) + " dB a 40 Hz.";
            t.objectiu = "Més volum útil i greus més nets.";
            t.accio = { Accio::EqCampana, 40.0f, g, 0.8f };
            t.gravetat = d;
            culpables (t, sub);
            v.push_back (t);
        }
        {
            int m = mitjos.a;
            for (int b = mitjos.a; b <= mitjos.b; ++b) if (c[(size_t) b] - referencia (b) > c[(size_t) m] - referencia (m)) m = b;
            const float d = c[(size_t) m] - referencia (m);
            const float veinsD = ((m > 0 ? c[(size_t) m - 1] - referencia (m - 1) : d) + (m < numBandes - 1 ? c[(size_t) m + 1] - referencia (m + 1) : d)) * 0.5f;
            if (d > 3.5f && d - veinsD > 2.0f)
            {
                Troballa t; t.mena = Mena::Oportunitat; t.confianca = Confianca::Mitjana;
                t.titol = "Ressonància als mitjos";
                t.on = "Un pic estret al voltant de " + hz (freqBanda (m)) + " (" + signe (d - veinsD) + " dB respecte del voltant).";
                t.perque = "Un pic estret als mitjos sona nasal o de «caixa».";
                t.eina = "EQ de màster";
                t.canvi = "-1,5 dB a " + hz (freqBanda (m)) + ", Q 2 (estret).";
                t.objectiu = "Mitjos més suaus.";
                t.accio = { Accio::EqCampana, freqBanda (m), -1.5f, 2.0f };
                t.gravetat = d - veinsD;
                culpables (t, { m, m, "" });
                v.push_back (t);
            }
        }
        if (const float d = desv (presencia); d < -3.0f)
        {
            Troballa t; t.mena = Mena::Oportunitat; t.confianca = conf (d, 5.0f);
            const float f = pic (presencia, false), g = clampf (-0.4f * d, 1.0f, 2.5f);
            t.titol = "Falta presència";
            t.on = "Entre 1,6 i 4 kHz, sobretot cap a " + hz (f) + " (" + signe (d) + " dB).";
            t.perque = "La veu i les guitarres poden quedar enrere i la cançó, poc definida.";
            t.eina = "EQ de màster";
            t.canvi = signe (g) + " dB a " + hz (f) + ", Q ample (0,7).";
            t.objectiu = "Més definició i veu més a prop.";
            t.accio = { Accio::EqCampana, f, g, 0.7f };
            t.gravetat = -d;
            v.push_back (t);
        }
        else if (d > 3.0f)
        {
            Troballa t; t.mena = Mena::Problema; t.confianca = conf (d, 5.0f);
            const float f = pic (presencia, true), g = -clampf (0.5f * d, 1.0f, 3.0f);
            t.titol = "Duresa";
            t.on = "Entre 1,6 i 4 kHz, sobretot cap a " + hz (f) + " (" + signe (d) + " dB).";
            t.perque = "És la zona on l'oïda és més sensible: massa energia cansa i fa el so agressiu.";
            t.eina = "EQ de màster";
            t.canvi = num (g) + " dB a " + hz (f) + ", Q 1,2. Si només molesta en alguns moments, millor una banda dinàmica.";
            t.objectiu = "Un so menys cansat.";
            t.accio = { Accio::EqCampana, f, g, 1.2f };
            t.gravetat = d + 1.0f;
            culpables (t, presencia);
            v.push_back (t);
        }
        if (const float d = desv (brillantor); d > 3.0f)
        {
            Troballa t; t.mena = Mena::Problema; t.confianca = conf (d, 5.0f);
            const float f = pic (brillantor, true);
            t.titol = "Brillantor excessiva (S i plats)";
            t.on = "Entre 5 i 8 kHz, sobretot cap a " + hz (f) + " (" + signe (d) + " dB).";
            t.perque = "Les S, els plats i els hi-hats poden punxar i cansar.";
            t.eina = "De-esser de màster";
            t.canvi = "Freqüència " + hz (f) + ", llindar -30 dB (que retalli 2–3 dB només quan punxa).";
            t.objectiu = "Calmar el que punxa sense apagar la cançó.";
            t.accio = { Accio::DeEsser, f };
            t.gravetat = d + 1.0f;
            culpables (t, brillantor);
            v.push_back (t);
        }
        else if (d < -4.0f)
        {
            Troballa t; t.mena = Mena::Oportunitat; t.confianca = Confianca::Mitjana;
            const float g = clampf (-0.3f * d, 1.0f, 2.5f);
            t.titol = "So apagat";
            t.on = "Entre 5 i 8 kHz (" + signe (d) + " dB).";
            t.perque = "Pot sonar fosc i amb poca definició dels plats i de les consonants.";
            t.eina = "EQ de màster (prestatge d'aire)";
            t.canvi = signe (g) + " dB al prestatge alt (12 kHz).";
            t.objectiu = "Més llum.";
            t.accio = { Accio::EqAire, 12000.0f, g };
            t.gravetat = -d - 1.0f;
            v.push_back (t);
        }
        if (const float d = desv (aire); d < -5.0f)
        {
            Troballa t; t.mena = Mena::Oportunitat; t.confianca = Confianca::Mitjana;
            const float g = clampf (-0.3f * d, 1.0f, 3.0f);
            t.titol = "Falta aire";
            t.on = "Per sobre de 10 kHz (" + signe (d) + " dB).";
            t.perque = "Sense aire, el so pot semblar tancat. Però també és una qüestió de gust (hi ha produccions fosques a propòsit).";
            t.eina = "EQ de màster (prestatge d'aire)";
            t.canvi = signe (g) + " dB al prestatge alt (12 kHz).";
            t.objectiu = "Més obert i modern.";
            t.accio = { Accio::EqAire, 12000.0f, g };
            t.gravetat = -d - 2.0f;
            v.push_back (t);
        }
        else if (d > 4.0f)
        {
            Troballa t; t.mena = Mena::Preferencia; t.confianca = Confianca::Baixa;
            t.titol = "Molt d'aire";
            t.on = "Per sobre de 10 kHz (" + signe (d) + " dB).";
            t.perque = "És una qüestió de gust; en alguns equips pot sonar fi o xiulat.";
            t.eina = "EQ de màster (prestatge d'aire)";
            t.canvi = "-1,5 dB al prestatge alt (12 kHz).";
            t.objectiu = "Un agut més rodó.";
            t.accio = { Accio::EqAire, 12000.0f, -1.5f };
            t.gravetat = d - 3.0f;
            v.push_back (t);
        }

        // ---- estèreo ----
        {
            double m = 0, s = 0;
            for (int b = 0; b <= 6; ++b) { m += mix.mid[(size_t) b]; s += mix.side[(size_t) b]; }
            const float sm = m > 0 ? (float) (10.0 * std::log10 (std::max (s, 1e-20) / m)) : -100.0f;
            if (sm > -12.0f)
            {
                Troballa t; t.mena = Mena::Problema; t.confianca = sm > -6.0f ? Confianca::Alta : Confianca::Mitjana;
                t.titol = "Greus als costats";
                t.on = "Per sota de 125 Hz, el Side és a " + num (sm) + " dB del Mid.";
                t.perque = "Els greus oberts es perden en mono (mòbils, ràdio, discoteca) i fan el baix inestable.";
                t.eina = "Low End Focus (Mono)";
                t.canvi = "Mono per sota de 120 Hz.";
                t.objectiu = "Baix i bombo al mig i compatibles amb mono.";
                t.accio = { Accio::Mono, 120.0f };
                t.gravetat = sm + 14.0f;
                culpables (t, { 0, 6, "" }, true);
                v.push_back (t);
            }
        }
        if (mix.correlacio < 0.1f)
        {
            Troballa t; t.mena = Mena::Problema; t.confianca = Confianca::Alta;
            t.titol = "Fase: correlació molt baixa";
            t.on = "Tota la cançó (correlació " + num (mix.correlacio, 2) + ").";
            t.perque = "En mono es perdrà part del so. Pot ser un efecte molt ample o una pista amb la fase girada.";
            t.eina = "Estèreo (Escolta en mono) i revisar les pistes";
            t.canvi = "Escolta en mono; si alguna cosa desapareix, revisa aquella pista.";
            t.objectiu = "Que soni bé també en mono.";
            t.gravetat = 6.0f;
            v.push_back (t);
        }
        else if (mix.sideMidDb < -18.0f)
        {
            Troballa t; t.mena = Mena::Preferencia; t.confianca = Confianca::Baixa;
            t.titol = "Mescla estreta";
            t.on = mix.sideMidDb < -60.0f ? std::string ("La mescla és mono (no hi ha res als costats).") : "El Side és a " + num (mix.sideMidDb) + " dB del Mid.";
            t.perque = "Pot sonar petita, però és una decisió de mescla i de gust.";
            t.eina = "Estèreo (amplada dels aguts)";
            t.canvi = "+15 % d'amplada als aguts.";
            t.objectiu = "Una mica més d'espai sense tocar el centre.";
            t.accio = { Accio::Amplada };
            t.gravetat = 1.0f;
            v.push_back (t);
        }

        // ---- dinàmica i nivell de la mescla ----
        const float plrMix = mix.loud.truePeakMax - mix.loud.integrated;
        if (plrMix > 0.0f && plrMix < 8.0f)
        {
            Troballa t; t.mena = Mena::Problema; t.confianca = plrMix < 6.0f ? Confianca::Alta : Confianca::Mitjana;
            t.titol = "La mescla ja ve molt comprimida";
            t.on = "Entre els pics i el volum mitjà hi ha només " + num (plrMix) + " dB.";
            t.perque = "Si al màster s'hi afegeix més compressió i limitació, perdrà el cop del tot.";
            t.eina = "Compressor i limitador del màster";
            t.canvi = "Comprimeix molt poc al màster (preset «Dinàmica natural» o «Només limitador») i revisa els compressors i limitadors del bus de la mescla.";
            t.objectiu = "No perdre el cop que queda.";
            t.gravetat = 9.0f - plrMix;
            v.push_back (t);
        }
        if (mix.loud.clips > 0 || mix.loud.truePeakMax > -0.3f)
        {
            Troballa t; t.mena = Mena::Oportunitat; t.confianca = Confianca::Baixa;
            t.titol = "La mescla arriba al sostre";
            t.on = "Pic de " + num (mix.loud.truePeakMax) + " dBTP abans del màster.";
            t.perque = "Mibase treballa amb coma flotant i no distorsiona per dins, però alguns plugins treballen millor amb marge.";
            t.eina = "Volum de les pistes o del bus";
            t.canvi = "Baixa la mescla 3–6 dB abans del Mastering Studio (el limitador ja pujarà el volum al final).";
            t.objectiu = "Marge per als efectes del màster.";
            t.gravetat = 0.5f;
            v.push_back (t);
        }

        // ---- el màster (si s'ha renderitzat) ----
        if (inf.ambMaster)
        {
            const auto& m = inf.master;
            if (ctx.lufsObjectiu < -0.5f && m.loud.integrated > -70.0f)
            {
                const float d = m.loud.integrated - ctx.lufsObjectiu;
                if (std::abs (d) > 1.0f)
                {
                    Troballa t; t.mena = Mena::Problema; t.confianca = Confianca::Alta;
                    t.titol = d > 0 ? "El màster és més fort que el perfil" : "El màster és més fluix que el perfil";
                    t.on = "Integrated " + num (m.loud.integrated) + " LUFS; el perfil " + ctx.perfil + " vol " + num (ctx.lufsObjectiu, 0) + ".";
                    t.perque = d > 0 ? "Les plataformes l'abaixaran igualment: només hi perds dinàmica." : "Sonarà més fluix que altres cançons del mateix estil.";
                    t.eina = "Limitador";
                    t.canvi = (d > 0 ? "Baixa " : "Puja ") + num (std::abs (d)) + " dB el volum: el guany del limitador i, si ja és a 0, l'Entrada del rack (després, comprova-ho amb «Ajusta al perfil» mentre sona).";
                    t.objectiu = "Arribar al perfil triat.";
                    t.accio = { Accio::Loudness, 0, -d };
                    t.accio.sostre = ctx.truePeakObjectiu;
                    t.gravetat = std::abs (d) + 1.0f;
                    v.push_back (t);
                }
            }
            if (m.loud.truePeakMax > ctx.truePeakObjectiu + 0.1f)
            {
                Troballa t; t.mena = Mena::Problema; t.confianca = Confianca::Alta;
                t.titol = "True peak per sobre del perfil";
                t.on = "El màster arriba a " + num (m.loud.truePeakMax) + " dBTP (el perfil: " + num (ctx.truePeakObjectiu) + ").";
                t.perque = "En passar a MP3/AAC pot distorsionar.";
                t.eina = "Limitador (sostre i True Peak)";
                t.canvi = "Sostre a " + num (ctx.truePeakObjectiu) + " dBTP amb True Peak encès.";
                t.objectiu = "Cap pic per sobre del perfil.";
                t.accio = { Accio::Loudness, 0, 0.0f };
                t.accio.sostre = ctx.truePeakObjectiu;
                t.gravetat = 5.0f;
                v.push_back (t);
            }
            const float perdut = mix.loud.lra - m.loud.lra, plrM = m.loud.truePeakMax - m.loud.integrated;
            if ((mix.loud.lra > 3.0f && perdut > 3.0f) || (plrM > 0.0f && plrM < 6.0f && plrMix > 8.0f))
            {
                Troballa t; t.mena = Mena::Problema; t.confianca = Confianca::Mitjana;
                t.titol = "El màster aixafa la cançó";
                t.on = "Rang (LRA) de " + num (mix.loud.lra) + " a " + num (m.loud.lra) + " LU; pics sobre el volum: " + num (plrM) + " dB.";
                t.perque = "Les parts fluixes i les fortes s'assemblen massa i la bateria perd el cop.";
                t.eina = "Macro Loudness, compressor i clipper";
                t.canvi = "Baixa el macro Loudness o tria un perfil més fluix; si cal volum, reparteix la feina amb el clipper.";
                t.objectiu = "Conservar la dinàmica de la cançó.";
                t.gravetat = 4.0f + perdut * 0.5f;
                v.push_back (t);
            }
        }

        // 1.2: el pas del mode guiat
        for (auto& t : v)
        {
            if (t.titol == "La mescla ja ve molt comprimida" || t.titol == "El màster aixafa la cançó") t.pas = 3;
            else if (t.titol == "Greus als costats" || t.titol.rfind ("Fase", 0) == 0 || t.titol == "Mescla estreta") t.pas = 5;
            else if (t.titol == "La mescla arriba al sostre" || t.titol.rfind ("El màster és", 0) == 0 || t.titol == "True peak per sobre del perfil") t.pas = 6;
            else t.pas = 2;
            t.delMaster = t.titol == "El màster aixafa la cançó" || t.titol.rfind ("El màster és", 0) == 0 || t.titol == "True peak per sobre del perfil";
        }
        // ordre: problemes, oportunitats, preferències; dins de cada mena, per gravetat
        std::stable_sort (v.begin(), v.end(), [] (const Troballa& a, const Troballa& b)
        {
            if (a.mena != b.mena) return (int) a.mena < (int) b.mena;
            return a.gravetat > b.gravetat;
        });

        // confiança general
        if (mix.segons >= 60.0) { inf.confianca = Confianca::Alta; inf.confiancaPerque = "S'ha escoltat tota la cançó (" + std::to_string ((int) mix.segons) + " s)."; }
        else if (mix.segons >= 20.0) { inf.confianca = Confianca::Mitjana; inf.confiancaPerque = "Només " + std::to_string ((int) mix.segons) + " s de música."; }
        else { inf.confianca = Confianca::Baixa; inf.confiancaPerque = "Molt poca música (" + std::to_string ((int) mix.segons) + " s)."; }
        if (ctx.ambReferencia) inf.confiancaPerque += " El to es compara amb la teva referència, «" + ctx.nomRef + "» (al mateix volum).";
        else inf.confiancaPerque += " El to es compara amb una corba de referència orientativa (pop/rock d'avui): és un punt de partida, no una norma.";
        if (inf.pistesAnalitzades == 0) inf.confiancaPerque += " Sense les pistes, no pot dir d'on ve cada problema.";

        // què faria un enginyer de màster
        {
            std::string e;
            std::vector<const Troballa*> mescla, master;
            for (auto& t : v)
            {
                if (t.mena == Mena::Preferencia) continue;
                (t.deMescla ? mescla : master).push_back (&t);
            }
            if (! mescla.empty())
            {
                e += "Abans de masteritzar, tornaria a la mescla: ";
                for (size_t k = 0; k < mescla.size(); ++k)
                    e += (k ? "; " : "") + mescla[k]->titol + " (sobretot «" + mescla[k]->culpables.front().pista + "»)";
                e += ". El màster ho toca tot alhora; a la pista només toques el que molesta. ";
            }
            if (master.empty())
                e += "La mescla està prou equilibrada: hi faria poca cosa. Una mica de cohesió amb el compressor i el limitador fins al perfil, i comparar amb el volum igualat.";
            else
            {
                e += "Al màster, per ordre: ";
                int k = 0;
                for (auto* t : master)
                {
                    if (k == 3) break;
                    std::string cv = t->canvi.substr (0, t->canvi.find (". "));
                    if (! cv.empty() && cv.back() == '.') cv.pop_back();
                    e += (k ? "; " : "") + t->titol + " (" + cv + ")";
                    ++k;
                }
                e += ". Faria els canvis petits, d'un en un, escoltant amb el volum igualat; i deixaria el loudness per al final.";
            }
            inf.enginyer = e;
        }
        return inf;
    }

    // ---------------------------------------------------------------
    namespace
    {
        int troba (const rack::Rack& r, const char* clau) { for (int i = 0; i < r.numModuls(); ++i) if (r.modul (i)->clau == clau) return i; return -1; }
        /** On va un mòdul nou: abans del clipper, del limitador i del mesurador (que han de ser els últims). */
        int posAbansFinal (const rack::Rack& r)
        {
            for (int i = 0; i < r.numModuls(); ++i)
            {
                const auto& k = r.modul (i)->clau;
                if (k == "clipper" || k == "limitm" || k == "limitador" || k == "loudness") return i;
            }
            return r.numModuls();
        }
        eq::Equalitzador* eqMaster (rack::Rack& r)
        {
            int i = troba (r, "eqm");
            if (i < 0) i = r.afegeix ("eqm", 0);
            return i >= 0 ? dynamic_cast<eq::Equalitzador*> (r.modul (i)->fx.get()) : nullptr;
        }
    }

    float freqOctava (int o)
    {
        static const float f[numOctaves] = { 40, 80, 160, 315, 630, 1250, 2500, 5000, 12500 };
        return f[std::clamp (o, 0, numOctaves - 1)];
    }

    std::array<float, numOctaves> diferencia (const Analisi& master, const Analisi& ref)
    {
        std::array<float, numOctaves> d {};
        if (! master.valid || ! ref.valid) return d;
        const auto cm = master.corba(), cr = ref.corba();
        for (int o = 0; o < numOctaves; ++o)
        {
            const int b0 = 3 * o, b1 = o == numOctaves - 1 ? numBandes - 1 : 3 * o + 2;
            float s = 0;
            for (int b = b0; b <= b1; ++b) s += cr[(size_t) b] - cm[(size_t) b];
            d[(size_t) o] = s / (float) (b1 - b0 + 1);
        }
        return d;
    }

    void aplicaMatching (rack::Rack& r, const Analisi& master, const Analisi& ref, float quantitat)
    {
        int i = troba (r, "eqmatch");
        if (quantitat <= 0.0f) { if (i >= 0) { r.treu (i); r.marcaModificat(); } return; }
        const auto d = diferencia (master, ref);
        std::array<float, numOctaves> abans {};
        if (i < 0)
        {
            const int e = troba (r, "eqm");
            i = r.afegeix ("eqmatch", e >= 0 ? e + 1 : 0);
        }
        auto* q = i >= 0 ? dynamic_cast<eq::Equalitzador*> (r.modul (i)->fx.get()) : nullptr;
        if (q == nullptr) return;
        for (int o = 0; o < numOctaves; ++o) { const auto b = q->banda (o); if (b.usada && b.tipus == eq::Bell) abans[(size_t) o] = b.guany; }
        for (int o = 0; o < numOctaves; ++o)
        {
            eq::Banda b;
            b.usada = true; b.tipus = eq::Bell; b.freq = freqOctava (o); b.q = 1.4f;
            b.guany = clampf (quantitat * (d[(size_t) o] + abans[(size_t) o]), -6.0f, 6.0f);
            q->posaBanda (o, b);
        }
        r.posaOn (i, true);
        r.reancoraMacros();   // 1.12
        r.marcaModificat();
    }

    bool aplica (rack::Rack& r, const Accio& a)
    {
        bool fet = false;
        switch (a.tipus)
        {
            case Accio::Cap: return false;
            case Accio::EqGreus: case Accio::EqAire:
            {
                auto* q = eqMaster (r);
                if (q == nullptr) return false;
                const int k = a.tipus == Accio::EqGreus ? 1 : 5;
                auto b = q->banda (k);
                if (! b.usada || b.tipus != (a.tipus == Accio::EqGreus ? eq::LowShelf : eq::HighShelf))
                {
                    b = {};
                    b.usada = true; b.tipus = a.tipus == Accio::EqGreus ? eq::LowShelf : eq::HighShelf; b.freq = a.freq; b.guany = 0.0f;
                }
                b.guany = clampf (b.guany + a.guany, -eq::guanyMax, eq::guanyMax);
                q->posaBanda (k, b);
                fet = true;
                break;
            }
            case Accio::EqCampana:
            {
                auto* q = eqMaster (r);
                if (q == nullptr) return false;
                int k = -1;
                // 1.12: només les bandes de l'assistent (de la 6 en amunt): les 0–5 són les del preset i els macros hi van enllaçats
                for (int i = 6; i < eq::maxBandes && k < 0; ++i)   // una campana que ja hi és, a menys de mitja octava
                {
                    const auto b = q->banda (i);
                    if (b.usada && b.tipus == eq::Bell && std::abs (std::log2 (b.freq / a.freq)) < 0.5f) k = i;
                }
                if (k < 0) for (int i = 6; i < eq::maxBandes && k < 0; ++i) if (! q->banda (i).usada) k = i;
                if (k < 0) return false;
                auto b = q->banda (k);
                const float abans = b.usada ? b.guany : 0.0f;
                b.usada = true; b.activa = true; b.tipus = eq::Bell; b.freq = a.freq; b.q = a.q;
                b.guany = clampf (abans + a.guany, -eq::guanyMax, eq::guanyMax);
                q->posaBanda (k, b);
                fet = true;
                break;
            }
            case Accio::DeEsser:
            {
                int i = troba (r, "deessm");
                const bool nou = i < 0;
                if (nou) { const int e = troba (r, "eqm"); i = r.afegeix ("deessm", e >= 0 ? e + 1 : 0); }
                auto* d = i >= 0 ? dynamic_cast<dinamica::Dinamica*> (r.modul (i)->fx.get()) : nullptr;
                if (d == nullptr) return false;
                d->posa (dinamica::FreqDeEsser, a.freq);
                if (! nou) d->posa (dinamica::Llindar, d->valor (dinamica::Llindar) - 3.0f);
                r.posaOn (i, true);
                fet = true;
                break;
            }
            case Accio::Mono:
            {
                int i = troba (r, "lowend");
                if (i >= 0) { dynamic_cast<lowend::LowEnd*> (r.modul (i)->fx.get())->posa (lowend::Mono, a.freq); r.posaOn (i, true); fet = true; break; }
                i = troba (r, "estereo");
                if (i >= 0) { dynamic_cast<estereo::Estereo*> (r.modul (i)->fx.get())->posa (estereo::MonoGreus, a.freq); r.posaOn (i, true); fet = true; break; }
                i = r.afegeix ("lowend", posAbansFinal (r));
                if (i >= 0) { dynamic_cast<lowend::LowEnd*> (r.modul (i)->fx.get())->posa (lowend::Mono, a.freq); fet = true; }
                break;
            }
            case Accio::Amplada:
            {
                int i = troba (r, "estereo");
                if (i < 0) i = r.afegeix ("estereo", posAbansFinal (r));
                auto* e = i >= 0 ? dynamic_cast<estereo::Estereo*> (r.modul (i)->fx.get()) : nullptr;
                if (e == nullptr) return false;
                e->posa (estereo::AmpAguts, e->valor (estereo::AmpAguts) + 15.0f);
                r.posaOn (i, true);
                fet = true;
                break;
            }
            case Accio::Loudness:
            {
                if (rack::limitadorFinal (r) < 0) r.afegeix ("limitm", troba (r, "loudness") >= 0 ? troba (r, "loudness") : -1);
                if (rack::limitadorFinal (r) < 0) return false;
                if (std::abs (a.guany) > 0.01f) rack::posaGuanyTotal (r, rack::guanyTotal (r) + a.guany);   // 1.12: també per abaixar
                rack::posaSostre (r, a.sostre);
                fet = true;
                break;
            }
        }
        if (fet) { r.reancoraMacros(); r.marcaModificat(); }   // 1.12: que tocar un macro no desfaci el canvi
        return fet;
    }
}
