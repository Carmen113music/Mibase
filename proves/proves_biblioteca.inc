// 0.27: proves de la biblioteca de samples (índex, categories, cerca, ordre).
#include "mibase/Biblioteca.h"

namespace
{
    void provesBiblioteca()
    {
        namespace bb = mibase::biblioteca;
        prova ("biblioteca: categories i BPM pel nom", []
        {
            comprova (bb::categoriaPerNom ("Kick 01.wav") == "Bombo", "Kick → Bombo");
            comprova (bb::categoriaPerNom ("snare_tight.wav") == "Caixa", "snare → Caixa");
            comprova (bb::categoriaPerNom ("Open Hat.wav") == "Hi-hat", "hat → Hi-hat");
            comprova (bb::categoriaPerNom ("vocal_gol.wav") == "Veu", "vocal → Veu");
            comprova (bb::categoriaPerNom ("Bass 808.wav") == "Baix", "808 → Baix");
            comprova (bb::categoriaPerNom ("riser long.wav") == "FX", "riser → FX");
            comprova (bb::categoriaPerNom ("Gooool.wav") == "Veu", "gol → Veu");
            comprova (bb::categoriaPerNom ("Res.wav") == "Altres", "la resta → Altres");
            comprova (bb::bpmPerNom ("Loop 120bpm.wav") == 120.0 && bb::bpmPerNom ("groove_95_BPM.wav") == 95.0
                      && bb::bpmPerNom ("bpm 128 drums.wav") == 128.0 && bb::bpmPerNom ("Kick 01.wav") == 0.0, "BPM pel nom");
        });

        prova ("biblioteca: one-shot o loop (pel nom i pel so)", []
        {
            comprova (bb::formaPerNom ("Drum Loop 01.wav") == bb::loop && bb::formaPerNom ("groove_95_BPM.wav") == bb::loop
                      && bb::formaPerNom ("Snare one shot.wav") == bb::oneShot && bb::formaPerNom ("Gooool.wav").empty(), "pel nom");
            const double fs = 48000.0;
            double bpm = 0.0;
            // un cop que s'apaga
            std::vector<float> cop ((size_t) (1.5 * fs));
            for (size_t i = 0; i < cop.size(); ++i) cop[i] = (float) (std::sin (0.05 * (double) i) * std::exp (-(double) i / fs * 6.0));
            comprova (bb::detectaForma (cop.data(), (long long) cop.size(), fs, bpm) == bb::oneShot, "un cop que s'apaga: one-shot");
            // un so curt
            comprova (bb::detectaForma (cop.data(), (long long) (0.5 * fs), fs, bpm) == bb::oneShot, "curt: one-shot");
            // un loop de bateria de 2 compassos a 120 (8 temps en 4 s), tallat sense apagar-se
            std::vector<float> lp ((size_t) (4.0 * fs));
            for (size_t i = 0; i < lp.size(); ++i)
            {
                const double t = (double) i / fs, dins = std::fmod (t, 0.5);
                lp[i] = (float) (0.8 * std::sin (2.0 * 3.14159265358979 * 60.0 * dins) * std::exp (-dins * 12.0) + 0.05 * std::sin (0.9 * (double) i));
            }
            comprova (bb::detectaForma (lp.data(), (long long) lp.size(), fs, bpm) == bb::loop, "un loop de bateria: loop");
            comprova (std::abs (bpm - 120.0) < 0.5, "i n'estima el BPM per la durada (" + std::to_string (bpm) + ")");
            // una nota llarga tallada (un pad): un sol atac
            std::vector<float> pad ((size_t) (3.0 * fs));
            for (size_t i = 0; i < pad.size(); ++i) pad[i] = (float) (0.4 * std::sin (0.03 * (double) i) * std::min (1.0, (double) i / (0.2 * fs)));
            comprova (bb::detectaForma (pad.data(), (long long) pad.size(), fs, bpm) == bb::oneShot, "un pad tallat: one-shot");
        });

        prova ("biblioteca: índex al disc, sincronitzar, cercar i ordenar", []
        {
            namespace fs = std::filesystem;
            const auto dir = fs::temp_directory_path() / "mibase_prova_biblioteca";
            fs::remove_all (dir);
            fs::create_directories (dir / "Bateria");
            for (auto* n : { "Gooool.wav", "Bateria/Kick 01.wav", "Bateria/Snare.wav", "notes.txt" }) std::ofstream (dir / mibase::disc::cami (n)) << "x";
            bb::Index idx;
            auto nous = bb::sincronitza (dir, idx);
            comprova (nous.size() == 3 && nous[0] == "Bateria/Kick 01.wav" && nous[2] == "Gooool.wav", "troba els 3 àudios (i no el txt), amb camins amb /");
            long long data = 1000;
            for (auto& f : nous)
            {
                bb::Sample s;
                s.fitxer = f; s.nom = fs::path (f).stem().string(); s.categoria = bb::categoriaPerNom (f);
                s.durada = f == "Gooool.wav" ? 1.3 : 0.4; s.data = data++;
                idx.samples.push_back (s);
            }
            idx.busca ("Gooool.wav")->etiquetes = bb::etiquetesDeText ("futbol, crit; Veu");
            idx.busca ("Gooool.wav")->tonalitat = "A3";
            idx.busca ("Gooool.wav")->forma = bb::oneShot;
            idx.busca ("Bateria/Kick 01.wav")->forma = bb::loop;
            comprova (bb::desa (dir, idx), "desa l'índex");
            auto r = bb::llegeix (dir);
            comprova (r.samples.size() == 3 && r.busca ("Gooool.wav") != nullptr && r.busca ("Gooool.wav")->etiquetes.size() == 3
                      && r.busca ("Gooool.wav")->tonalitat == "A3", "el rellegeix igual");
            comprova (bb::sincronitza (dir, r).empty(), "res de nou");
            fs::remove (dir / "Bateria" / "Snare.wav");
            bb::sincronitza (dir, r);
            comprova (r.samples.size() == 2 && r.busca ("Bateria/Snare.wav") == nullptr, "treu els que ja no hi són");

            auto f = bb::filtra (r, "fútbol", "", bb::Ordre::Nom, false);
            comprova (f.size() == 1 && r.samples[(size_t) f[0]].fitxer == "Gooool.wav", "cerca per etiqueta sense accents");
            comprova (bb::filtra (r, "kick 01", "", bb::Ordre::Nom, false).size() == 1, "cerca amb diverses paraules");
            comprova (bb::filtra (r, "", "Bombo", bb::Ordre::Nom, false).size() == 1, "filtra per categoria");
            comprova (r.busca ("Gooool.wav")->forma == bb::oneShot, "es desa si és one-shot o loop");
            comprova (bb::filtra (r, "", "", bb::Ordre::Nom, false, bb::oneShot).size() == 1 && bb::filtra (r, "loop", "", bb::Ordre::Nom, false).size() == 1,
                      "filtra i cerca per one-shot / loop");
            auto d = bb::filtra (r, "", "", bb::Ordre::Durada, true);
            comprova (d.size() == 2 && r.samples[(size_t) d[0]].fitxer == "Gooool.wav", "ordena per durada (de més a menys)");
            auto n = bb::filtra (r, "", "", bb::Ordre::Nom, false);
            comprova (r.samples[(size_t) n[0]].nom == "Gooool", "ordena per nom");
            comprova (bb::nomLliure (dir, "Gooool", ".wav") == "Gooool 2.wav" && bb::nomLliure (dir, "Nou", ".wav") == "Nou.wav", "nom lliure");
            fs::remove_all (dir);
        });
    }
}
