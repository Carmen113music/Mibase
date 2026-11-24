// 0.94: Mastering Studio (rack de màster, loudness, Estèreo, A/B igualat) i acords per clips.
#include "mibase/Estereo.h"
#include "mibase/Loudness.h"
#include "mibase/Rack.h"
#include "mibase/Limitador.h"
namespace
{
    struct Estereo094 { std::vector<float> L, R; };
    Estereo094 sinus094 (double fs, double segons, double hz, double dbfs, double fase = 0.0, bool antifase = false)
    {
        Estereo094 s;
        const size_t n = (size_t) (fs * segons);
        const double a = std::pow (10.0, dbfs / 20.0);
        s.L.resize (n); s.R.resize (n);
        for (size_t i = 0; i < n; ++i)
        {
            const float v = (float) (a * std::sin (2.0 * 3.14159265358979 * hz * (double) i / fs + fase));
            s.L[i] = v; s.R[i] = antifase ? -v : v;
        }
        return s;
    }
    Estereo094 soroll094 (double fs, double segons, double dbfs, unsigned llavor = 12345u)
    {
        Estereo094 s;
        const size_t n = (size_t) (fs * segons);
        const double a = std::pow (10.0, dbfs / 20.0);
        s.L.resize (n); s.R.resize (n);
        float bl = 0, br = 0;   // soroll una mica rosat (més greus), estèreo i correlat a mitges
        for (size_t i = 0; i < n; ++i)
        {
            llavor = llavor * 1664525u + 1013904223u; const float x = (float) ((llavor >> 8) & 0xFFFF) / 32768.0f - 1.0f;
            llavor = llavor * 1664525u + 1013904223u; const float y = (float) ((llavor >> 8) & 0xFFFF) / 32768.0f - 1.0f;
            bl = 0.97f * bl + 0.25f * x; br = 0.97f * br + 0.25f * (0.6f * x + 0.4f * y);
            s.L[i] = (float) (a * (bl + 0.3f * x)); s.R[i] = (float) (a * (br + 0.3f * y));
        }
        return s;
    }
    void afegeix094 (Estereo094& a, const Estereo094& b) { a.L.insert (a.L.end(), b.L.begin(), b.L.end()); a.R.insert (a.R.end(), b.R.begin(), b.R.end()); }
    void processa094 (mibase::Efecte& fx, Estereo094& s, int bloc = 512)
    {
        for (size_t a = 0; a < s.L.size(); a += (size_t) bloc)
            fx.processa (s.L.data() + a, s.R.data() + a, (int) std::min ((size_t) bloc, s.L.size() - a));
    }

    void proves094()
    {
        using namespace mibase;
        prova ("0.94 acords per clips: clip buit toca el seu tros, clip amb notes no, fora dels clips res", []
        {
            using namespace mibase::edicio;
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Pad", "midi");
            afegeixPista (q, "Acords", "acords");
            const int M = esAcords (q.pistes[0]) ? 1 : 0, A = 1 - M;
            auto acord = [&] (const char* nom, double a, double b) { Clip k; k.acord = nom; k.pos = a; k.inici = 0.0; k.fi = b - a; q.pistes[(size_t) A].clips.push_back (k); };
            acord ("Am", 0.0, 4.0);
            acord ("F", 4.0, 8.0);
            auto& t = q.pistes[(size_t) M];
            t.segueixAcords = true;
            comprova (notesAcordsSols (q, t).size() == 6, "sense clips: tota la cançó");
            Clip c; c.midi = true; c.pos = 0.0; c.inici = 0.0; c.fi = 4.0;
            t.clips.push_back (c);
            auto n = notesAcordsSols (q, t);
            bool nomesAm = n.size() == 3;
            for (auto& x : n) nomesAm = nomesAm && x.ini == 0.0 && x.fi < 4.0;
            comprova (nomesAm, "un clip buit de 0 a 4 s: només el La menor (" + std::to_string (n.size()) + ")");
            Clip d = c; d.pos = 4.0; d.notes.push_back ({ 0, 480, 60, 100, 0 });
            t.clips.push_back (d);
            n = notesAcordsSols (q, t);
            comprova (n.size() == 3, "el clip amb notes no toca els acords");
            auto r = resumAcordsClips (t);
            comprova (r.clips == 2 && r.acords == 1 && r.notes == 1, "resum: 1 toca els acords, 1 té notes");
            // un clip buit llarg tapat per un clip amb notes al mig
            t.clips.clear();
            Clip llarg = c; llarg.fi = 8.0; t.clips.push_back (llarg);
            Clip sobre = c; sobre.pos = 2.0; sobre.fi = 4.0; sobre.notes.push_back ({ 0, 480, 60, 100, 0 }); t.clips.push_back (sobre);
            n = notesAcordsSols (q, t);
            bool trams = n.size() == 6;
            for (auto& x : n) trams = trams && ((x.ini == 0.0 && x.fi <= 2.0) || (x.ini == 6.0 && x.fi < 8.0));
            comprova (trams, "tapat al mig: Am de 0 a 2 s i F de 6 a 8 s");
            t.clips.clear();
            Clip mut = c; mut.mut = true; mut.fi = 8.0; t.clips.push_back (mut);
            comprova (notesAcordsSols (q, t).empty() && ! tocaAcordsSols (t), "només un clip silenciat: res");
        });

        prova ("0.94 loudness: sinus de 1 kHz a -23 dBFS = -23 LUFS, crest de 3 dB, correlació 1", []
        {
            auto s = sinus094 (48000, 10, 1000, -23);
            const auto r = loudness::analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
            comprova (std::abs (r.integrated + 23.0f) < 0.2f, "integrated " + std::to_string (r.integrated));
            comprova (std::abs (r.shortMax + 23.0f) < 0.2f && std::abs (r.momentaryMax + 23.0f) < 0.3f, "short i momentary");
            comprova (r.lra < 0.5f, "LRA gairebé 0 (" + std::to_string (r.lra) + ")");
            comprova (std::abs (r.samplePeakMax + 23.0f) < 0.1f && std::abs (r.truePeakMax + 23.0f) < 0.3f, "pics a -23");
            comprova (r.correlacio > 0.99f && r.clips == 0, "mono: correlació 1, sense clips");
            loudness::Mesurador m; m.prepara (48000); m.processa (s.L.data(), s.R.data(), (int) s.L.size());
            comprova (std::abs (m.mesures().crest.load() - 3.01f) < 0.2f, "crest " + std::to_string (m.mesures().crest.load()));
            auto s44 = sinus094 (44100, 6, 1000, -23);
            const auto r44 = loudness::analitza (s44.L.data(), s44.R.data(), s44.L.size(), 44100);
            comprova (std::abs (r44.integrated + 23.0f) < 0.2f, "també a 44,1 kHz (" + std::to_string (r44.integrated) + ")");
        });

        prova ("0.94 loudness: portes de l'Integrated i LRA (EBU 3342)", []
        {
            auto s = sinus094 (48000, 10, 1000, -23);
            afegeix094 (s, sinus094 (48000, 10, 1000, -200));   // silenci: la porta absoluta el treu
            afegeix094 (s, sinus094 (48000, 5, 1000, -45));     // 22 dB per sota: la porta relativa el treu
            auto r = loudness::analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
            comprova (std::abs (r.integrated + 23.0f) < 0.3f, "integrated amb silenci i un tros fluix: " + std::to_string (r.integrated));
            auto l = sinus094 (48000, 20, 1000, -20);
            afegeix094 (l, sinus094 (48000, 20, 1000, -30));
            r = loudness::analitza (l.L.data(), l.R.data(), l.L.size(), 48000);
            comprova (std::abs (r.lra - 10.0f) < 1.0f, "20 s a -20 i 20 s a -30: LRA 10 (" + std::to_string (r.lra) + ")");
        });

        prova ("0.94 loudness: true peak entre mostres i clips", []
        {
            auto s = sinus094 (48000, 2, 12000, 0.0, 3.14159265358979 / 4.0);   // les mostres cauen a ±0,707
            auto r = loudness::analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
            comprova (std::abs (r.samplePeakMax + 3.01f) < 0.1f, "sample peak -3 dB (" + std::to_string (r.samplePeakMax) + ")");
            comprova (r.truePeakMax > -0.6f && r.truePeakMax < 0.3f, "true peak ~0 dBTP (" + std::to_string (r.truePeakMax) + ")");
            auto c = sinus094 (48000, 1, 1000, 3.0);
            for (auto& v : c.L) v = std::clamp (v, -1.0f, 1.0f);
            for (auto& v : c.R) v = std::clamp (v, -1.0f, 1.0f);
            comprova (loudness::analitza (c.L.data(), c.R.data(), c.L.size(), 48000).clips > 100, "detecta el clipping");
        });

        prova ("0.94 Estèreo: neutre per defecte, amplada 0 = mono, mono als greus", []
        {
            using namespace mibase::estereo;
            auto s = soroll094 (48000, 1, -12);
            auto o = s;
            Estereo e; e.prepara (48000, 512);
            processa094 (e, o);
            float dif = 0;
            for (size_t i = 0; i < s.L.size(); ++i) dif = std::max (dif, std::max (std::abs (o.L[i] - s.L[i]), std::abs (o.R[i] - s.R[i])));
            comprova (dif < 1.0e-4f, "tot al 100 %: surt igual (" + std::to_string (dif) + ")");
            e.posa (Amplada, 0);
            auto m = s; e.prepara (48000, 512); processa094 (e, m);
            float lr = 0;
            for (size_t i = 4800; i < m.L.size(); ++i) lr = std::max (lr, std::abs (m.L[i] - m.R[i]));
            comprova (lr < 1.0e-4f && e.mesures().correlacioOut.load() > 0.99f, "amplada 0: mono i correlació 1 (" + std::to_string (lr) + ", " + std::to_string (e.mesures().correlacioOut.load()) + ")");
            Estereo g; g.posa (MonoGreus, 120); g.prepara (48000, 512);
            auto greu = sinus094 (48000, 1, 50, -6, 0.0, true), agut = sinus094 (48000, 1, 2000, -6, 0.0, true);
            processa094 (g, greu); g.buida(); processa094 (g, agut);
            comprova (pic (greu.L, 24000, 48000) < 0.5f * 0.05f, "50 Hz als costats: desapareix (" + std::to_string (pic (greu.L, 24000, 48000)) + ")");
            comprova (pic (agut.L, 24000, 48000) > 0.45f, "2 kHz als costats: es queda");
            Estereo ample; ample.posa (AmpAguts, 200); ample.prepara (48000, 512);
            auto a = soroll094 (48000, 2, -12); processa094 (ample, a);
            comprova (ample.mesures().sideMidOut.load() > ample.mesures().sideMidIn.load() + 1.0f, "obrir els aguts puja el Side");
        });

        prova ("0.94 Mastering Studio: presets reals, macros connectats, limitador al final i true peak sota el sostre", []
        {
            using namespace mibase::rack;
            const auto& ps = presetsFabrica (Tipus::Master);
            comprova (ps.size() >= 20, "almenys 20 presets (" + std::to_string (ps.size()) + ")");
            comprova (categories (Tipus::Master).size() == 3 && presetsFabrica().front().nom != ps.front().nom, "categories pròpies, separades del Vocal Studio");
            int malament = 0;
            std::string quin;
            for (const auto& p : ps)
            {
                Rack r (Tipus::Master);
                r.posaEstat (p.estat);
                bool ok = r.numModuls() >= 3 && r.modul (r.numModuls() - 1)->clau == "limitm" && r.estat() == p.estat;
                for (int m = 0; m < numMacros; ++m)
                {
                    bool te = false;
                    for (const auto& e : r.enllacos()) te = te || e.macro == m;
                    if (m != 0 && m != 2 && m != 5) ok = ok && te;   // Escalfor, Punch i Densitat depenen de si hi ha saturació o compressor
                }
                r.prepara (48000, 512);
                auto s = soroll094 (48000, 4, -14);
                processa094 (r, s);
                const auto lm = loudness::analitza (s.L.data() + 96000, s.R.data() + 96000, s.L.size() - 96000, 48000);
                ok = ok && totFinit (s.L) && totFinit (s.R) && lm.truePeakMax <= -0.4f;
                if (! ok) { ++malament; quin += p.nom + " (TP " + std::to_string (lm.truePeakMax) + ") "; }
            }
            comprova (malament == 0, "tots els presets bé: " + quin);
            Rack v;
            comprova (! v.esMaster() && v.estat().find ("\"tipus\"") == std::string::npos && v.nomMacro (0) == "Cos", "el Vocal Studio no canvia");
            Rack mm (Tipus::Master);
            comprova (mm.nomMacro (7) == "Loudness" && mm.loudnessSortida() != nullptr && v.loudnessSortida() == nullptr, "macros i mesures del màster");
        });

        prova ("0.94 Mastering Studio: el macro Loudness puja el volum; A/B original i volum igualat", []
        {
            using namespace mibase::rack;
            auto mesura = [] (Rack& r, float macroLoud, bool original, bool igualat)
            {
                r.posaEstat (presetsFabrica (Tipus::Master).front().estat);
                r.posaMacro (7, macroLoud);
                r.posaIgualat (igualat);
                r.posaOriginal (original);
                r.prepara (48000, 512);
                auto s = soroll094 (48000, 12, -20);
                processa094 (r, s);
                return loudness::analitza (s.L.data() + 48000 * 6, s.R.data() + 48000 * 6, s.L.size() - 48000 * 6, 48000);
            };
            auto ent = soroll094 (48000, 12, -20);
            const float lIn = loudness::analitza (ent.L.data() + 48000 * 6, ent.R.data() + 48000 * 6, ent.L.size() - 48000 * 6, 48000).integrated;
            Rack a (Tipus::Master), b (Tipus::Master), c (Tipus::Master), d (Tipus::Master);
            const float fluix = mesura (a, 0.1f, false, false).integrated, fort = mesura (b, 0.9f, false, false).integrated;
            comprova (fort > fluix + 4.0f, "Loudness al 9 sona més fort que al 1 (" + std::to_string (fluix) + " → " + std::to_string (fort) + ")");
            const float orig = mesura (c, 0.9f, true, false).integrated;
            comprova (std::abs (orig - lIn) < 0.3f, "Original: sona el mix d'entrada (" + std::to_string (orig) + " / " + std::to_string (lIn) + ")");
            const float igual = mesura (d, 0.9f, false, true).integrated;
            comprova (std::abs (igual - lIn) < 1.5f, "Igualat: el processat baixa fins al volum de l'original (" + std::to_string (igual) + ")");
            comprova (d.compensacioDb() < -3.0f, "la compensació es veu (" + std::to_string (d.compensacioDb()) + " dB)");
            comprova (d.loudnessSortida()->integrated.load() > lIn + 3.0f, "el mesurador de sortida diu el volum real del màster, no l'igualat");
        });
    }
}
