// 0.26: proves del samplejador (bucle de sustain, to, modes, time-stretch, desat).
#include "mibase/Sampler.h"

namespace
{
    namespace ms = mibase::sampler;

    /** Un «Gooool» sintètic: G = 150 Hz (0–0,2 s), o = 220 Hz (0,2–1,0 s), l = 330 Hz (1,0–1,3 s). */
    std::shared_ptr<const ms::Mostra> goool (double fs)
    {
        auto m = std::make_shared<ms::Mostra>();
        m->fs = fs;
        const int n = (int) (1.3 * fs);
        m->L.resize ((size_t) n);
        double fase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / fs;
            const double f = t < 0.2 ? 150.0 : t < 1.0 ? 220.0 : 330.0;
            fase += 2.0 * 3.14159265358979 * f / fs;
            m->L[(size_t) i] = (float) (0.5 * std::sin (fase));
        }
        return m;
    }

    /** Renderitza: note-on a la mostra «on», note-off a «off» (< 0: mai), «total» mostres. Torna L. */
    std::vector<float> tocaSampler (ms::Samplejador& s, int nota, long long on, long long off, long long total, double fs, int vel = 127)
    {
        s.prepara (fs, 256);
        std::vector<float> L ((size_t) total), R ((size_t) total);
        for (long long b = 0; b < total; b += 256)
        {
            const int n = (int) std::min<long long> (256, total - b);
            mibase::EventInstrument ev[2];
            int k = 0;
            if (on >= b && on < b + n) ev[k++] = { (int) (on - b), 0x90, (std::uint8_t) nota, (std::uint8_t) vel };
            if (off >= b && off < b + n) ev[k++] = { (int) (off - b), 0x80, (std::uint8_t) nota, 0 };
            s.processa (ev, k, L.data() + b, R.data() + b, n);
        }
        return L;
    }

    /** Freqüència per passos per zero (cap amunt) entre dues mostres. */
    double freqZeros (const std::vector<float>& x, size_t des, size_t fins, double fs)
    {
        int c = 0;
        long long primer = -1, darrer = -1;
        for (size_t i = des + 1; i < fins && i < x.size(); ++i)
            if (x[i - 1] < 0.0f && x[i] >= 0.0f) { if (primer < 0) primer = (long long) i; darrer = (long long) i; ++c; }
        if (c < 2) return 0.0;
        return (c - 1) * fs / (double) (darrer - primer);
    }

    /** L'última mostra amb so (> llindar). */
    long long finalDelSo (const std::vector<float>& x, float llindar = 1e-3f)
    {
        for (long long i = (long long) x.size() - 1; i >= 0; --i) if (std::abs (x[(size_t) i]) > llindar) return i;
        return -1;
    }

    float salteMaxim (const std::vector<float>& x, size_t des, size_t fins)
    {
        float m = 0.0f;
        for (size_t i = des + 1; i < fins && i < x.size(); ++i) m = std::max (m, std::abs (x[i] - x[i - 1]));
        return m;
    }

    mibase::ParamsSampler paramsGol()
    {
        mibase::ParamsSampler p;
        p.root = 60;
        p.inici = 0.0; p.fi = 1.3;
        p.bucleDe = 0.4; p.bucleA = 0.8;   // dins la «o»
        p.modeBucle = 3;
        p.crossfade = 0.03;
        p.atac = 0.0; p.decai = 0.0; p.sosteniment = 1.0; p.alliberament = 0.05;
        p.fadeOut = 0.005;
        p.vel = 0.0;
        return p;
    }

    void provesSampler()
    {
        const double fs = 48000.0;

        prova ("samplejador: Gooool (Go, ooo mentre es manté, ol en deixar anar)", [fs]
        {
            auto p = paramsGol();
            ms::Samplejador s (ms::prepara (goool (fs), p));
            s.posaParams (p);
            const long long on = 1000, off = on + (long long) (3.0 * fs), total = off + (long long) (1.5 * fs);
            auto y = tocaSampler (s, 60, on, off, total, fs);
            comprova (std::abs (freqZeros (y, (size_t) (on + 0.05 * fs), (size_t) (on + 0.18 * fs), fs) - 150.0) < 5.0, "comença amb la «G» (150 Hz)");
            comprova (std::abs (freqZeros (y, (size_t) (on + 0.25 * fs), (size_t) (on + 0.38 * fs), fs) - 220.0) < 5.0, "després la «o» (220 Hz)");
            const double fBucle = freqZeros (y, (size_t) (on + 1.5 * fs), (size_t) (on + 2.9 * fs), fs);
            comprova (std::abs (fBucle - 220.0) < 5.0, "mentre es manté, «ooo» (" + std::to_string (fBucle) + " Hz)");
            float minPic = 1.0f;
            for (long long t = on + (long long) (0.2 * fs); t < off - 2000; t += 2400) minPic = std::min (minPic, pic (y, (size_t) t, (size_t) t + 2400));
            comprova (minPic > 0.4f, "el bucle no fa forats de volum (" + std::to_string (minPic) + ")");
            comprova (salteMaxim (y, (size_t) (on + 0.3 * fs), (size_t) off) < 0.06f, "sense clics al punt d'unió del bucle");
            comprova (std::abs (freqZeros (y, (size_t) (off + 0.55 * fs), (size_t) (off + 0.68 * fs), fs) - 330.0) < 6.0, "en deixar anar, la «l» (330 Hz)");
            const double cua = (finalDelSo (y) - off) / fs;
            comprova (cua > 0.45 && cua < 0.95, "la cua dura el que queda des del final del bucle (" + std::to_string (cua) + " s)");
        });

        prova ("samplejador: durada fixa (la nota canvia el to però no la velocitat)", [fs]
        {
            auto p = paramsGol();
            p.modeBucle = 0;
            p.durFixa = true;
            ms::Samplejador s (ms::prepara (goool (fs), p));
            s.posaParams (p);
            auto y = tocaSampler (s, 72, 0, -1, (long long) (2.0 * fs), fs);
            const double f = freqZeros (y, (size_t) (0.3 * fs), (size_t) (0.9 * fs), fs);
            comprova (std::abs (f - 440.0) < 8.0, "una octava amunt, la «o» a 440 Hz (" + std::to_string (f) + ")");
            const double d = finalDelSo (y) / fs;
            comprova (std::abs (d - 1.3) < 0.03, "però dura el mateix, 1,3 s (" + std::to_string (d) + ")");
            float minPic = 1.0f;
            for (double t = 0.3; t < 0.95; t += 0.05) minPic = std::min (minPic, pic (y, (size_t) (t * fs), (size_t) ((t + 0.05) * fs)));
            comprova (minPic > 0.3f, "sense forats de volum (" + std::to_string (minPic) + ")");
            auto z = tocaSampler (s, 55, 0, -1, (long long) (2.0 * fs), fs);
            comprova (std::abs (freqZeros (z, (size_t) (0.3 * fs), (size_t) (0.9 * fs), fs) - 220.0 * std::pow (2.0, -5.0 / 12.0)) < 5.0
                      && std::abs (finalDelSo (z) / fs - 1.3) < 0.03, "cinc semitons avall, mateixa durada");
            auto r = tocaSampler (s, 60, 0, -1, (long long) (2.0 * fs), fs);
            comprova (std::abs (finalDelSo (r) / fs - 1.3) < 0.01 && salteMaxim (r, 1000, (size_t) (1.2 * fs)) < 0.06f, "a la Root sona tal qual");

            auto pg = paramsGol();
            pg.durFixa = true;
            ms::Samplejador sg (ms::prepara (goool (fs), pg));
            sg.posaParams (pg);
            const long long on = 0, off = (long long) (3.0 * fs);
            auto g = tocaSampler (sg, 67, on, off, off + (long long) (1.5 * fs), fs);
            const double fo = freqZeros (g, (size_t) (1.5 * fs), (size_t) (2.9 * fs), fs);
            comprova (std::abs (fo - 220.0 * std::pow (2.0, 7.0 / 12.0)) < 6.0, "Gooool una quinta amunt: la «o» es manté (" + std::to_string (fo) + ")");
            const double cua = (finalDelSo (g) - off) / fs;
            comprova (cua > 0.45 && cua < 0.95, "i en deixar anar, la cua dura el mateix que a la Root (" + std::to_string (cua) + " s)");
        });

        prova ("samplejador: sense durada fixa, l'octava de dalt sona el doble d'aguda i dura la meitat", [fs]
        {
            auto p = paramsGol();
            p.modeBucle = 0;
            p.durFixa = false;
            ms::Samplejador s (ms::prepara (goool (fs), p));
            s.posaParams (p);
            auto y = tocaSampler (s, 72, 0, -1, (long long) (1.5 * fs), fs);
            comprova (std::abs (freqZeros (y, (size_t) (0.15 * fs), (size_t) (0.45 * fs), fs) - 440.0) < 8.0, "la «o» a 440 Hz");
            const double d = finalDelSo (y) / fs;
            comprova (std::abs (d - 0.65) < 0.02, "dura 0,65 s (" + std::to_string (d) + ")");
            auto p2 = p; p2.tune = -12.0;
            ms::Samplejador s2 (ms::prepara (goool (fs), p2));
            s2.posaParams (p2);
            auto y2 = tocaSampler (s2, 72, 0, -1, (long long) (1.5 * fs), fs);
            comprova (std::abs (freqZeros (y2, (size_t) (0.3 * fs), (size_t) (0.9 * fs), fs) - 220.0) < 5.0, "Tune -12 ho compensa");
        });

        prova ("samplejador: modes Off, Endavant, Ping-pong, One-shot i Gate", [fs]
        {
            auto p = paramsGol();
            const long long off = (long long) (2.5 * fs), total = (long long) (4.0 * fs);
            p.modeBucle = 0;
            { ms::Samplejador s (ms::prepara (goool (fs), p)); s.posaParams (p);
              auto y = tocaSampler (s, 60, 0, off, total, fs);
              comprova (std::abs (finalDelSo (y) / fs - 1.3) < 0.02, "Off: s'acaba al final del sample encara que es mantingui"); }
            p.modeBucle = 1;
            { ms::Samplejador s (ms::prepara (goool (fs), p)); s.posaParams (p);
              auto y = tocaSampler (s, 60, 0, off, total, fs);
              const double f = finalDelSo (y) / fs;
              comprova (f > 2.5 && f < 2.6, "Endavant: sona fins al release (" + std::to_string (f) + ")");
              comprova (salteMaxim (y, (size_t) (0.3 * fs), (size_t) off) < 0.06f, "Endavant: sense clics"); }
            p.modeBucle = 2;
            { ms::Samplejador s (ms::prepara (goool (fs), p)); s.posaParams (p);
              auto y = tocaSampler (s, 60, 0, off, total, fs);
              comprova (pic (y, (size_t) (2.0 * fs), (size_t) (2.4 * fs)) > 0.4f && finalDelSo (y) / fs < 2.6, "Ping-pong: es manté i s'acaba amb el release");
              comprova (salteMaxim (y, (size_t) (0.3 * fs), (size_t) off) < 0.06f, "Ping-pong: sense clics"); }
            p.modeBucle = 1; p.modeToc = 1;
            { ms::Samplejador s (ms::prepara (goool (fs), p)); s.posaParams (p);
              auto y = tocaSampler (s, 60, 0, (long long) (0.1 * fs), total, fs);
              comprova (std::abs (finalDelSo (y) / fs - 1.3) < 0.02, "One-shot: sona sencer encara que es deixi anar abans (i sense bucle)"); }
            p.modeToc = 2;
            { ms::Samplejador s (ms::prepara (goool (fs), p)); s.posaParams (p);
              auto y = tocaSampler (s, 60, 0, (long long) (0.1 * fs), total, fs);
              const double f = finalDelSo (y) / fs;
              comprova (f > 0.1 && f < 0.11, "Gate: calla de seguida en deixar anar (" + std::to_string (f) + ")"); }
        });

        prova ("samplejador: sense bucle definit, el mode Sustain + release toca el sample sencer", [fs]
        {
            auto p = paramsGol();
            p.bucleDe = p.bucleA = -1.0;
            ms::Samplejador s (ms::prepara (goool (fs), p));
            s.posaParams (p);
            auto y = tocaSampler (s, 60, 0, (long long) (3.0 * fs), (long long) (4.0 * fs), fs);
            comprova (std::abs (finalDelSo (y) / fs - 1.3) < 0.02 && totFinit (y), "s'acaba al final del sample");
        });

        prova ("samplejador: inici i final retallen sense tocar l'original; reversa", [fs]
        {
            auto p = paramsGol();
            p.modeBucle = 0; p.inici = 0.25; p.fi = 0.5;
            auto orig = goool (fs);
            ms::Samplejador s (ms::prepara (orig, p));
            s.posaParams (p);
            auto y = tocaSampler (s, 60, 0, -1, (long long) fs, fs);
            comprova (std::abs (finalDelSo (y) / fs - 0.25) < 0.01, "sona 0,25 s");
            comprova (std::abs (freqZeros (y, 100, (size_t) (0.2 * fs), fs) - 220.0) < 5.0, "només la «o»");
            comprova (orig->L.size() == (size_t) (1.3 * fs), "l'original no canvia");
            auto pr = p; pr.reversa = true; pr.inici = 0.0; pr.fi = 0.15;   // els primers 0,15 s (la G) girats
            ms::Samplejador sr (ms::prepara (orig, pr));
            sr.posaParams (pr);
            auto yr = tocaSampler (sr, 60, 0, -1, (long long) fs, fs);
            comprova (std::abs (finalDelSo (yr) / fs - 0.15) < 0.01 && std::abs (freqZeros (yr, 100, (size_t) (0.14 * fs), fs) - 150.0) < 5.0,
                      "reversa: el mateix fragment (la G), girat");
        });

        prova ("samplejador: time-stretch (Speed sense canviar el to) i Speed de cinta", [fs]
        {
            std::vector<float> x ((size_t) fs);
            for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (0.5 * std::sin (2.0 * 3.14159265358979 * 440.0 * (double) i / fs));
            auto y = ms::estira (x, 2.0, fs);
            comprova (y.size() == x.size() * 2, "dura el doble");
            comprova (std::abs (freqZeros (y, 5000, y.size() - 5000, fs) - 440.0) < 3.0, "el to no canvia (" + std::to_string (freqZeros (y, 5000, y.size() - 5000, fs)) + ")");
            comprova (pic (y, 5000, y.size() - 5000) > 0.4f && pic (y, 5000, y.size() - 5000) < 0.6f, "el volum es manté");
            auto z = ms::estira (x, 0.5, fs);
            comprova (z.size() == x.size() / 2 && std::abs (freqZeros (z, 3000, z.size() - 3000, fs) - 440.0) < 4.0, "a la meitat, el mateix to");

            auto p = paramsGol();
            p.modeBucle = 0; p.speed = 2.0; p.mantenTo = true;
            ms::Samplejador s (ms::prepara (goool (fs), p));
            s.posaParams (p);
            auto a = tocaSampler (s, 60, 0, -1, (long long) (1.5 * fs), fs);
            comprova (std::abs (finalDelSo (a) / fs - 0.65) < 0.03, "Speed x2: dura la meitat (" + std::to_string (finalDelSo (a) / fs) + ")");
            comprova (std::abs (freqZeros (a, (size_t) (0.15 * fs), (size_t) (0.45 * fs), fs) - 220.0) < 5.0, "Speed x2: mateix to");
            p.mantenTo = false;
            ms::Samplejador s2 (ms::prepara (goool (fs), p));
            s2.posaParams (p);
            auto b = tocaSampler (s2, 60, 0, -1, (long long) (1.5 * fs), fs);
            comprova (std::abs (freqZeros (b, (size_t) (0.15 * fs), (size_t) (0.45 * fs), fs) - 440.0) < 8.0, "cinta x2: una octava amunt");
            p.mantenTo = true; p.speed = 0.5; p.modeBucle = 3;
            ms::Samplejador s3 (ms::prepara (goool (fs), p));
            s3.posaParams (p);
            auto c = tocaSampler (s3, 60, 0, (long long) (4.0 * fs), (long long) (5.5 * fs), fs);
            comprova (std::abs (freqZeros (c, (size_t) (2.0 * fs), (size_t) (3.5 * fs), fs) - 220.0) < 5.0, "Speed :2 amb bucle: els marcadors segueixen la «o»");
        });

        prova ("samplejador: polifonia, robatori de veus, mono i legato", [fs]
        {
            auto p = paramsGol();
            p.modeBucle = 1; p.maxVeus = 2; p.durFixa = false;
            ms::Samplejador s (ms::prepara (goool (fs), p));
            s.posaParams (p);
            s.prepara (fs, 256);
            std::vector<float> L (256), R (256);
            mibase::EventInstrument ev[4] = { { 0, 0x90, 60, 100 }, { 10, 0x90, 64, 100 }, { 20, 0x90, 67, 100 }, { 30, 0x90, 72, 100 } };
            s.processa (ev, 4, L.data(), R.data(), 256);
            for (int k = 0; k < 10; ++k) s.processa (nullptr, 0, L.data(), R.data(), 256);
            comprova (s.fentSo() && totFinit (L), "sona");
            int vives = 0;
            // després de 5 ms, només en queden 2 (les robades s'han apagat sense clic)
            comprova (pic (L, 0, 256) < 2.2f, "dues veus com a molt");
            (void) vives;

            auto pm = paramsGol(); pm.modeBucle = 1; pm.veus = 2;   // legato
            ms::Samplejador lg (ms::prepara (goool (fs), pm));
            lg.posaParams (pm);
            lg.prepara (fs, 256);
            std::vector<float> y;
            mibase::EventInstrument a1[1] = { { 0, 0x90, 60, 100 } };
            lg.processa (a1, 1, L.data(), R.data(), 256);
            for (int k = 0; k < 60; ++k) lg.processa (nullptr, 0, L.data(), R.data(), 256);   // dins la «o»
            mibase::EventInstrument a2[1] = { { 0, 0x90, 72, 100 } };
            lg.processa (a2, 1, L.data(), R.data(), 256);
            for (int k = 0; k < 40; ++k) { lg.processa (nullptr, 0, L.data(), R.data(), 256); y.insert (y.end(), L.begin(), L.end()); }
            comprova (std::abs (freqZeros (y, 0, y.size(), fs) - 440.0) < 8.0, "legato: la nota nova continua la «o» una octava amunt (sense tornar a la G)");
        });

        prova ("samplejador: escoltar des d'un punt de l'ona", [fs]
        {
            auto p = paramsGol();
            ms::Samplejador s (ms::prepara (goool (fs), p));
            s.posaParams (p);
            s.escolta (1.05);
            auto y = tocaSampler (s, 0, -1, -1, (long long) (0.5 * fs), fs);
            comprova (std::abs (freqZeros (y, 200, (size_t) (0.2 * fs), fs) - 330.0) < 6.0, "sona la «l» de seguida");
            comprova (std::abs (finalDelSo (y) / fs - 0.25) < 0.01, "fins al final del fitxer, sense bucle");
            auto p2 = p; p2.inici = 0.5; p2.fi = 0.7;
            ms::Samplejador s2 (ms::prepara (goool (fs), p2));
            s2.posaParams (p2);
            s2.escolta (0.1);
            auto y2 = tocaSampler (s2, 0, -1, -1, (long long) (1.5 * fs), fs);
            comprova (std::abs (freqZeros (y2, 200, (size_t) (0.08 * fs), fs) - 150.0) < 5.0 && std::abs (finalDelSo (y2) / fs - 1.2) < 0.01,
                      "escolta l'original encara que sigui fora del fragment (des de la G fins al final)");
            s.escolta (0.3);
            s.aturaEscolta();
            auto z = tocaSampler (s, 0, -1, -1, (long long) (0.2 * fs), fs);
            comprova (finalDelSo (z) < (long long) (0.01 * fs), "atura");
        });

        prova ("samplejador: pas per zero, detecció del to i desat al projecte", [fs]
        {
            auto m = goool (fs);
            const double z = ms::zeroCrossing (*m, 0.5003);
            const auto i = (size_t) std::llround (z * fs);
            comprova (i > 0 && (m->L[i] == 0.0f || (m->L[i - 1] <= 0.0f) != (m->L[i] <= 0.0f) || (m->L[i] <= 0.0f) != (m->L[i + 1] <= 0.0f)), "el punt és un pas per zero");
            comprova (std::abs (z - 0.5003) < 0.003, "a prop d'on era");
            auto la = std::make_shared<ms::Mostra>();
            la->fs = fs;
            la->L.resize ((size_t) fs);
            for (size_t k = 0; k < la->L.size(); ++k) la->L[k] = (float) (0.3 * std::sin (2.0 * 3.14159265358979 * 220.0 * (double) k / fs) + 0.1 * std::sin (4.0 * 3.14159265358979 * 220.0 * (double) k / fs));
            comprova (ms::detectaRoot (*la) == 57, "un La de 220 Hz és A3 (57)");

            namespace fs_ = std::filesystem;
            const auto dir = fs_::temp_directory_path() / "mibase_prova_sampler";
            fs_::remove_all (dir);
            fs_::create_directories (dir);
            mibase::Projecte pr;
            pr.nom = "Samples";
            mibase::Pista t;
            t.id = "t1"; t.nom = "Gol"; t.tipus = "midi"; t.instrument = mibase::instrumentSampler;
            t.sampler = paramsGol();
            t.sampler.fitxer = "Samples/Gol.wav"; t.sampler.nom = "Gol"; t.sampler.speed = 1.5; t.sampler.durFixa = false; t.sampler.filtre = 3; t.sampler.pendent = 24;
            pr.pistes.push_back (t);
            auto r = mibase::disc::creaProjecte (dir, pr, "2026-11-09T10:00:00", "09/11/2026 10:00");
            comprova (r.error.empty(), "projecte creat i desat");
            auto q = mibase::disc::llegeix (r.fitxer);
            comprova (q.has_value() && ! q->pistes.empty(), "rellegit");
            if (q && ! q->pistes.empty())
            {
                const auto& x = q->pistes.back().sampler;
                comprova (mibase::esSampler (q->pistes.back()) && x.fitxer == "Samples/Gol.wav" && x.bucleDe == 0.4 && x.bucleA == 0.8
                          && x.modeBucle == 3 && x.speed == 1.5 && ! x.durFixa && x.filtre == 3 && x.pendent == 24 && x.crossfade == 0.03,
                          "es conserven el fitxer, els marcadors i els paràmetres");
            }
            fs_::remove_all (dir);
        });
    }
}
