// 1.5: Guitar Rack (amplis, pantalles, micròfons i pedals de guitarra).
#include "mibase/GuitarRack.h"
#include <set>
namespace
{
    /** Senyal de guitarra per a les proves: acords rasguejats (Karplus-Strong), pic ~0,6. */
    std::vector<float> guitarraGR (double fs, double segons, unsigned llavor = 7)
    {
        std::vector<float> out ((size_t) (fs * segons), 0.0f);
        std::mt19937 rng (llavor);
        std::uniform_real_distribution<float> u (-1.0f, 1.0f);
        const double acord[6] = { 82.41, 123.47, 164.81, 207.65, 246.94, 329.63 };
        for (double t = 0.0; t + 0.5 < segons; t += 1.0)
            for (int s = 0; s < 6; ++s)
            {
                const int N = (int) (fs / acord[s]);
                std::vector<float> d ((size_t) N);
                for (auto& x : d) x = u (rng) * 0.22f;
                const size_t ini = (size_t) ((t + s * 0.012) * fs), fi = std::min (out.size(), ini + (size_t) fs);
                int k = 0; float prev = 0.0f;
                for (size_t i = ini; i < fi; ++i)
                {
                    const float x = d[(size_t) k];
                    d[(size_t) k] = 0.5f * (x + prev) * 0.996f;
                    prev = x; k = (k + 1) % N;
                    out[i] += x;
                }
            }
        return out;
    }
    struct SoGR { std::vector<float> L, R; };
    SoGR passaGR (mibase::guitarrack::GuitarRack& r, const std::vector<float>& in, int bloc = 256)
    {
        SoGR s { in, in };
        for (size_t i = 0; i < in.size(); i += (size_t) bloc)
            r.processa (s.L.data() + i, s.R.data() + i, (int) std::min<size_t> ((size_t) bloc, in.size() - i));
        return s;
    }
    double rmsGR (const std::vector<float>& v, size_t a = 0, size_t b = 0)
    {
        if (b == 0 || b > v.size()) b = v.size();
        double e = 0.0;
        for (size_t i = a; i < b; ++i) e += (double) v[i] * v[i];
        return std::sqrt (e / (double) std::max<size_t> (1, b - a));
    }
    bool finitsGR (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
    float salt (const std::vector<float>& v, size_t a, size_t b)
    {
        float m = 0.0f;
        for (size_t i = std::max<size_t> (1, a); i < std::min (b, v.size()); ++i) m = std::max (m, std::abs (v[i] - v[i - 1]));
        return m;
    }
    /** Energia per sobre de f (aprox.: diferència de primer ordre filtrada). */
    double aguts (const std::vector<float>& v, size_t a, double fs, double f)
    {
        const double k = std::exp (-2.0 * 3.14159265 * f / fs);
        double lp = 0.0, e = 0.0;
        for (size_t i = a; i < v.size(); ++i) { lp = (1 - k) * v[i] + k * lp; const double h = v[i] - lp; e += h * h; }
        return std::sqrt (e / (double) (v.size() - a));
    }

    void proves105()
    {
        using namespace mibase::guitarrack;
        const double fs = 48000.0;

        prova ("1.5 GUITAR RACK: catàleg complet (19 amplis, 44 pedals, pantalles, altaveus i micros), claus úniques", []
        {
            comprova (amplis().size() >= 19, "amplis: " + std::to_string (amplis().size()));
            comprova (pedals().size() >= 44, "pedals: " + std::to_string (pedals().size()));
            comprova (cabs().size() >= 7 && altaveus().size() == 6 && micros().size() == 4, "pantalles, altaveus i micros");
            std::set<std::string> c;
            for (const auto& a : amplis())
            {
                c.insert (a.clau);
                comprova (! a.veus.empty() && (a.canals.empty() ? a.veus.size() == 1 : a.veus.size() == a.canals.size()), a.clau + ": una veu per canal");
                comprova ((int) a.canals.size() <= maxCanals, a.clau + ": canals");
                comprova (indexCab (a.cab) >= 0 && indexAltaveu (a.altaveu) >= 0, a.clau + ": pantalla i altaveu existeixen");
                comprova (! a.inspirat.empty() && ! a.etiqueta[AGain].empty(), a.clau + ": té nom real i guany");
            }
            comprova (c.size() == amplis().size(), "amplis únics");
            std::set<std::string> p;
            std::set<int> cats;
            for (const auto& m : pedals())
            {
                p.insert (m.clau);
                cats.insert (m.categoria);
                comprova (! m.controls.empty() && (int) m.controls.size() <= maxControls, m.clau + ": controls");
                comprova (creaPedal (m.clau) != nullptr, m.clau + ": es pot crear");
                comprova (textCerca (m).find (senseAccents (m.marca)) != std::string::npos, m.clau + ": es troba per la marca");
            }
            comprova (p.size() == pedals().size(), "pedals únics");
            comprova ((int) cats.size() == numCategories, "totes les categories tenen pedals");
            comprova (senseAccents ("Modulació Àmplia") == "modulacio amplia", "cerca sense accents");
        });

        prova ("1.5 GUITAR RACK: cada pedal sona (mono i estèreo) sense valors no vàlids, també amb els poms als extrems", [fs]
        {
            const auto in = guitarraGR (fs, 1.6);
            int dolents = 0, muts = 0;
            for (const auto& m : pedals())
                for (int extrem = 0; extrem < 3; ++extrem)
                {
                    auto p = creaPedal (m.clau);
                    for (int c = 0; c < p->numControls(); ++c)
                    {
                        const auto& k = m.controls[(size_t) c];
                        p->posa (c, extrem == 0 ? k.def : (extrem == 1 ? k.mn : k.mx));
                    }
                    p->prepara (fs, 256);
                    std::vector<float> L = in, R = in, M = in;
                    for (size_t i = 0; i < in.size(); i += 256)
                    {
                        const int n = (int) std::min<size_t> (256, in.size() - i);
                        p->passa (L.data() + i, R.data() + i, n);
                    }
                    auto q = creaPedal (m.clau);
                    for (int c = 0; c < q->numControls(); ++c) q->posa (c, p->valor (c));
                    q->prepara (fs, 256);
                    for (size_t i = 0; i < in.size(); i += 256) q->passa (M.data() + i, nullptr, (int) std::min<size_t> (256, in.size() - i));
                    if (! finitsGR (L) || ! finitsGR (R) || ! finitsGR (M) || pic (L, 0, L.size()) > 30.0f || pic (M, 0, M.size()) > 30.0f) { ++dolents; std::printf ("      %s (%d)\n", m.clau.c_str(), extrem); }
                    if (extrem == 0 && rmsGR (M, 4800) < 1e-4 && m.clau != "volume") { ++muts; std::printf ("      mut: %s\n", m.clau.c_str()); }
                }
            comprova (dolents == 0, "pedals amb valors no vàlids: " + std::to_string (dolents));
            comprova (muts == 0, "pedals muts amb els poms de sempre: " + std::to_string (muts));
        });

        prova ("1.5 GUITAR RACK: bypass de debò (un pedal apagat no toca el so) i encendre/apagar sense clics", [fs]
        {
            const auto in = guitarraGR (fs, 2.0);
            GuitarRack a, b;
            a.buidaPlaca(); b.buidaPlaca();
            for (const char* k : { "ts808", "ce2", "dd3", "rv6" }) { const int i = b.afegeix (k); b.pedal (i)->on.store (false); }
            a.prepara (fs, 256); b.prepara (fs, 256);
            const auto sa = passaGR (a, in), sb = passaGR (b, in);
            bool igual = true;
            for (size_t i = 0; i < in.size(); ++i) if (sa.L[i] != sb.L[i] || sa.R[i] != sb.R[i]) { igual = false; break; }
            comprova (igual, "amb els pedals apagats, el so és idèntic al d'una pedalera buida");
            // encendre i apagar un fuzz a mig so: els salts entre mostres no superen els del so encès o apagat de sempre
            auto render = [&] (int mode)   // 0 sempre encès, 1 sempre apagat, 2 commutant
            {
                GuitarRack c;
                c.buidaPlaca();
                const int k = c.afegeix ("muff");
                c.posa (AmpOn, 0.0f); c.posa (CabOn, 0.0f);
                c.pedal (k)->on.store (mode != 1);
                c.prepara (fs, 128);
                std::vector<float> L (in), R (in);
                for (size_t i = 0; i < in.size(); i += 128)
                {
                    if (mode == 2 && (i == 30080 || i == 60160 || i == 84992)) c.pedal (k)->on.store (! c.pedal (k)->on.load());
                    c.processa (L.data() + i, R.data() + i, (int) std::min<size_t> (128, in.size() - i));
                }
                return L;
            };
            const auto on = render (0), off = render (1), comm = render (2);
            float ref = 0.0f, mx = 0.0f;
            for (size_t at : { 30080u, 60160u, 84992u })
            {
                ref = std::max ({ ref, salt (on, at - 200, at + 1500), salt (off, at - 200, at + 1500) });
                mx = std::max (mx, salt (comm, at - 200, at + 1500));
            }
            comprova (mx <= ref * 1.1f + 0.01f, "sense clics en commutar: salt " + std::to_string (mx) + " (sense commutar " + std::to_string (ref) + ")");
        });

        prova ("1.5 GUITAR RACK: tots els amplis i canals sonen, al mateix volum (±4 dB), i els poms fan el que diuen", [fs]
        {
            const auto in = guitarraGR (fs, 3.0);
            double mn = 0.0, mx = -200.0;
            for (int a = 0; a < (int) amplis().size(); ++a)
                for (int c = 0; c < (int) amplis()[(size_t) a].veus.size(); ++c)
                {
                    GuitarRack r;
                    r.buidaPlaca(); r.posaAmpli (a); r.posa (Canal, (float) c); r.posa (Room, 0.0f); r.posa (AmpReverb, 0.0f);
                    r.posaCab (indexCab (amplis()[(size_t) a].cab));
                    r.prepara (fs, 256);
                    const auto s = passaGR (r, in);
                    comprova (finitsGR (s.L), amplis()[(size_t) a].clau + ": finit");
                    const double db = 20.0 * std::log10 (rmsGR (s.L, 4800) + 1e-12);
                    if (c > 0 || amplis()[(size_t) a].veus.size() == 1) { mn = std::min (mn == 0.0 ? db : mn, db); mx = std::max (mx, db); }
                }
            comprova (mx - mn < 8.0, "diferència de volum entre amplis: " + std::to_string (mx - mn) + " dB");
            // Treble: més aguts; Gain: més distorsió (menys diferència entre pics i RMS)
            auto mesura = [&] (int p, float v, bool ambCab)
            {
                GuitarRack r;
                r.buidaPlaca(); r.posaAmpli (indexAmpli ("jcm800")); r.posa (CabOn, ambCab ? 1.0f : 0.0f); r.posa (Room, 0.0f);
                r.posa (p, v);
                r.prepara (fs, 256);
                return passaGR (r, in).L;
            };
            const auto t2 = mesura (AmpTreble, 1.0f, false), t8 = mesura (AmpTreble, 9.0f, false);
            comprova (aguts (t8, 4800, fs, 3000.0) / rmsGR (t8, 4800) > 1.15 * aguts (t2, 4800, fs, 3000.0) / rmsGR (t2, 4800), "Treble alt = més aguts");
            const auto b2 = mesura (AmpBass, 1.0f, false), b8 = mesura (AmpBass, 9.0f, false);
            comprova (aguts (b2, 4800, fs, 400.0) / rmsGR (b2, 4800) > aguts (b8, 4800, fs, 400.0) / rmsGR (b8, 4800), "Bass alt = proporcionalment menys aguts");
            const auto g1 = mesura (AmpGain, 1.0f, true), g9 = mesura (AmpGain, 9.5f, true);
            const double cf1 = pic (g1, 4800, g1.size()) / rmsGR (g1, 4800), cf9 = pic (g9, 4800, g9.size()) / rmsGR (g9, 4800);
            comprova (cf9 < cf1 * 0.8, "Gain alt = més comprimit i saturat (factor de cresta " + std::to_string (cf1) + " → " + std::to_string (cf9) + ")");
        });

        prova ("1.5 GUITAR RACK: la pantalla i els micròfons canvien el so (posició, distància, angle, segon micro, sala)", [fs]
        {
            const auto in = guitarraGR (fs, 2.0);
            auto so = [&] (int p, float v)
            {
                GuitarRack r;
                r.buidaPlaca(); r.posaAmpli (indexAmpli ("jcm800")); r.posa (Room, 0.0f);
                r.posa (p, v);
                r.prepara (fs, 256);
                return passaGR (r, in);
            };
            const auto centre = so (PosA, 0.0f), vora = so (PosA, 1.0f);
            comprova (aguts (centre.L, 4800, fs, 3000.0) > 1.2 * aguts (vora.L, 4800, fs, 3000.0), "al centre del con, més aguts que a la vora");
            const auto recte = so (AngleA, 0.0f), girat = so (AngleA, 45.0f);
            comprova (aguts (recte.L, 4800, fs, 5000.0) > aguts (girat.L, 4800, fs, 5000.0), "girat, menys aguts");
            const auto sec = so (Room, 0.0f), sala = so (Room, 60.0f);
            double dif = 0.0;
            for (size_t i = 4800; i < sec.L.size(); ++i) dif += std::abs (sala.L[i] - sala.R[i]);
            comprova (dif > 1.0, "la sala obre l'estèreo");
            const auto b = so (MesclaB, 80.0f);
            comprova (finitsGR (b.L) && rmsGR (b.L, 4800) > 1e-3, "amb el micro B barrejat sona");
            for (int m = 0; m < 4; ++m) { const auto s = so (MicA, (float) m); comprova (finitsGR (s.L) && rmsGR (s.L, 4800) > 1e-3, "micro " + micros()[(size_t) m].clau); }
        });

        prova ("1.5 GUITAR RACK: canviar d'ampli, de canal o de pedals mentre sona no fa clic ni deixa valors no vàlids", [fs]
        {
            const auto in = guitarraGR (fs, 3.0);
            GuitarRack r;
            r.buidaPlaca();
            r.afegeix ("ts808");
            r.prepara (fs, 256);
            std::vector<float> L (in), R (in);
            float mx = 0.0f;
            for (size_t i = 0; i < in.size(); i += 256)
            {
                if (i == 256 * 40) r.posaAmpli (indexAmpli ("rectifier"));
                if (i == 256 * 80) r.posa (Canal, 0.0f);
                if (i == 256 * 120) r.afegeix ("ce2", -1, true);
                if (i == 256 * 160) r.mou (0, 1, true);
                if (i == 256 * 200) r.treu (0);
                if (i == 256 * 240) r.posaCab (indexCab ("fender212"));
                r.processa (L.data() + i, R.data() + i, (int) std::min<size_t> (256, in.size() - i));
                r.recull();
            }
            for (size_t at : { 40u, 80u, 120u, 160u, 200u, 240u }) mx = std::max (mx, salt (L, at * 256, at * 256 + 4800));
            comprova (finitsGR (L) && finitsGR (R), "tot finit");
            comprova (mx < 0.35f, "salt màxim en els canvis: " + std::to_string (mx));
            comprova (r.plaquesGuardades() <= 2, "les plaques velles s'alliberen: " + std::to_string (r.plaquesGuardades()));
        });

        prova ("1.5 GUITAR RACK: pedalera (afegir, treure, moure, loop d'efectes, màxim de pedals) i paràmetres per posició", []
        {
            GuitarRack r;
            r.buidaPlaca();
            const int a = r.afegeix ("ts808"), b = r.afegeix ("rv6", -1, true), c = r.afegeix ("ce2", 0);
            comprova (r.numPedals() == 3 && a >= 0 && b >= 0 && c == 0, "tres pedals");
            comprova (r.pedal (0)->model().clau == "ce2" && r.pedal (1)->model().clau == "ts808" && r.pedal (2)->model().clau == "rv6", "ordre: primer els d'abans de l'ampli, després el loop");
            r.mou (0, 2, true);
            comprova (r.pedal (2)->model().clau == "ce2" && r.pedal (2)->loop.load() && r.placa()->loop.back() == 1, "moure al loop");
            r.mou (2, 0, false);
            comprova (r.pedal (0)->model().clau == "ce2" && ! r.pedal (0)->loop.load(), "tornar davant de l'ampli");
            // paràmetres de la pedalera: el pom 1 del primer pedal (Chorus: Depth)
            const int i = paramPedal (0, 1);
            r.posaParametre (i, 1.0f);
            comprova (std::abs (r.pedal (0)->valor (1) - 10.0f) < 1e-4f, "automatitzar un pom d'un pedal");
            comprova (r.infoParametre (i).nom.find ("Depth") != std::string::npos && r.infoParametre (i).id == "p1c2", "nom i id del paràmetre");
            r.posaParametre (paramOnPedal (0), 0.0f);
            comprova (! r.pedal (0)->on.load(), "apagar un pedal amb l'automatització");
            for (int k = 0; k < 20; ++k) r.afegeix ("dd3");
            comprova (r.numPedals() == maxPedals, "màxim de pedals: " + std::to_string (r.numPedals()));
            r.treu (0);
            comprova (r.numPedals() == maxPedals - 1, "treure");
            comprova (r.numParametres() == numParams && r.infoParametre (AmpGain).nom.find ("Volume") != std::string::npos, "el Gain del Plexi es diu Volume");
        });

        prova ("1.5 GUITAR RACK: l'estat es desa i es recupera igual (projecte), i els presets de fàbrica", [fs]
        {
            GuitarRack r;
            r.posaEstat ("");
            comprova (r.numPedals() >= 1, "neix amb una pedalera a punt");
            r.posaAmpli (indexAmpli ("ac30"));
            r.posa (AmpTreble, 7.5f); r.posa (MicA, 2.0f); r.posa (Room, 33.0f); r.posaAltaveu (indexAltaveu ("creamback"));
            const int k = r.afegeix ("timeline", -1, true);
            r.pedal (k)->posa (1, 777.0f);
            r.pedal (k)->on.store (false);
            r.posaNomPreset ("Prova", true);
            const auto e = r.estat();
            GuitarRack s;
            s.posaEstat (e);
            comprova (s.estat() == e, "estat idèntic");
            comprova (s.ampli() == indexAmpli ("ac30") && std::abs (s.valor (AmpTreble) - 7.5f) < 1e-4f && s.altaveu() == indexAltaveu ("creamback"), "ampli, poms i altaveu");
            comprova (s.nomPreset() == "Prova" && s.presetModificat(), "nom del preset");
            const auto* p = s.pedal (s.numPedals() - 1);
            comprova (p != nullptr && p->model().clau == "timeline" && p->loop.load() && ! p->on.load() && std::abs (p->valor (1) - 777.0f) < 0.01f, "pedal recuperat");
            s.posaEstat ("això no és json");
            comprova (s.estat() == e, "un estat dolent no trenca res");

            const auto& ps = presetsFabrica();
            comprova (ps.size() >= 30, "presets de fàbrica: " + std::to_string (ps.size()));
            std::set<std::string> noms;
            for (const auto& x : ps) noms.insert (x.nom);
            comprova (noms.size() == ps.size(), "noms únics");
            for (const char* n : { "Clean Fender", "British Crunch", "Classic Rock Marshall", "Indie Rock Clean", "Indie Rock Crunch", "Shoegaze Atmosphere",
                                   "Ambient Guitar", "Post-Rock", "Modern Alternative", "Heavy Rock", "Vintage Blues", "80s Lead", "Stadium Rock", "Dreamy Chorus", "Radio Rock" })
                comprova (noms.count (n) == 1, std::string ("hi ha el preset ") + n);
            const auto in = guitarraGR (fs, 2.0);
            int dolents = 0;
            for (const auto& x : ps)
            {
                GuitarRack q;
                q.posaEstat (x.estat);
                q.prepara (fs, 512);
                const auto so = passaGR (q, in, 512);
                const double db = 20.0 * std::log10 (rmsGR (so.L, 4800) + 1e-12);
                if (! finitsGR (so.L) || pic (so.L, 0, so.L.size()) > 1.0f || db < -30.0 || db > -6.0) { ++dolents; std::printf ("      %s: %.1f dB\n", x.nom.c_str(), db); }
            }
            comprova (dolents == 0, "presets fora de rang: " + std::to_string (dolents));
        });

        prova ("1.5 GUITAR RACK: el gate talla el soroll i la sortida mai passa de 0 dBFS", [fs]
        {
            std::vector<float> soroll ((size_t) fs, 0.0f);
            std::mt19937 g (3);
            std::normal_distribution<float> nd (0.0f, 0.0003f);
            for (auto& x : soroll) x = nd (g);
            GuitarRack r;
            r.buidaPlaca(); r.posaAmpli (indexAmpli ("5150")); r.posa (Gate, -45.0f);
            r.prepara (fs, 256);
            const auto s = passaGR (r, soroll);
            comprova (rmsGR (s.L, 12000) < 1e-4, "amb el gate, el soroll no surt");
            GuitarRack q;
            q.buidaPlaca(); q.posaAmpli (indexAmpli ("5150")); q.posa (Sortida, 12.0f); q.posa (Entrada, 18.0f);
            q.prepara (fs, 256);
            const auto f = passaGR (q, guitarraGR (fs, 1.5));
            comprova (pic (f.L, 0, f.L.size()) <= 1.0f && pic (f.R, 0, f.R.size()) <= 1.0f, "protecció de sortida");
        });
    }
}
