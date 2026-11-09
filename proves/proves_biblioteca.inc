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
            auto d = bb::filtra (r, "", "", bb::Ordre::Durada, true);
            comprova (d.size() == 2 && r.samples[(size_t) d[0]].fitxer == "Gooool.wav", "ordena per durada (de més a menys)");
            auto n = bb::filtra (r, "", "", bb::Ordre::Nom, false);
            comprova (r.samples[(size_t) n[0]].nom == "Gooool", "ordena per nom");
            comprova (bb::nomLliure (dir, "Gooool", ".wav") == "Gooool 2.wav" && bb::nomLliure (dir, "Nou", ".wav") == "Nou.wav", "nom lliure");
            fs::remove_all (dir);
        });
    }
}
