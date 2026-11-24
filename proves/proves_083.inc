// 0.83: exportació amb opcions.
#include "mibase/Wav.h"
namespace
{
    void proves083()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        constexpr double fs = 48000.0;
        auto prop = [] (double a, double b, double e = 1.0e-6) { return std::abs (a - b) < e; };

        prova ("0.83 Exportar: només les pistes triades, sense efectes ni màster", [&]
        {
            Projecte p;
            auto& a = afegeixPista (p, "Veu", "audio");
            Insert x; x.id = "i1"; x.plugin = "intern:reverb"; a.inserts.push_back (x);
            Insert v; v.id = "i2"; v.plugin = pluginVeu; a.inserts.push_back (v);
            afegeixPista (p, "Bus", "fx");
            Enviament e; e.id = "v1"; e.desti = p.pistes[1].id; p.pistes[0].enviaments.push_back (e);
            const auto idBaix = afegeixPista (p, "Baix", "audio").id;
            const int ib = pistaPerId (p, idBaix);
            p.pistes[(size_t) ib].mut = true;
            Insert m; m.id = "i3"; m.plugin = "intern:limitador"; p.insertsMaster.push_back (m);
            afegeixGravacio (p, 0, "Àudios/a.wav", "a", 4.0, 1.0, 1.0, 0.0);
            afegeixGravacio (p, ib, "Àudios/b.wav", "b", 10.0, 0.0, 0.0, 0.0);
            std::pair<double, double> tros;
            OpcionsExport o;
            auto q = projectePerExportar (p, o, {}, tros);
            comprova (prop (tros.first, 1.0) && prop (tros.second, 5.0), "tal com sona: el baix silenciat no compta (1-5 s)");
            comprova (q.pistes[0].inserts.size() == 2 && q.insertsMaster.size() == 1 && ! q.clic, "amb efectes i màster, sense claqueta");
            q = projectePerExportar (p, o, { ib }, tros);
            comprova (q.pistes[(size_t) ib].solo && ! q.pistes[(size_t) ib].mut && ! q.pistes[0].solo, "triar el baix: sona sol (encara que estigués silenciat)");
            comprova (prop (tros.first, 0.0) && prop (tros.second, 10.0), "i el tros és el seu (0-10 s)");
            o.efectes = false; o.master = false;
            q = projectePerExportar (p, o, {}, tros);
            comprova (q.pistes[0].inserts.size() == 1 && esInsertVeu (q.pistes[0].inserts[0]), "sense efectes (el Vocal Editor es queda)");
            comprova (q.pistes[0].enviaments.empty() && q.insertsMaster.empty(), "sense enviaments ni màster");
            comprova (p.pistes[0].inserts.size() == 2 && p.pistes[(size_t) ib].mut, "el projecte de veritat no canvia");
        });

        prova ("0.83 Exportar: el bucle i una part", [&]
        {
            Projecte p;
            p.tempo = 120.0;
            afegeixPista (p, "Veu", "audio");
            afegeixGravacio (p, 0, "Àudios/a.wav", "a", 30.0, 0.0, 0.0, 0.0);
            marcaBucle (p, 2, 3);
            const auto idPart = creaPart (p, "Tornada", 4, 6, 0);
            std::pair<double, double> tros;
            OpcionsExport o;
            o.tros = OpcionsExport::Bucle;
            projectePerExportar (p, o, {}, tros);
            comprova (prop (tros.first, iniciBucle (p)) && prop (tros.second, fiBucle (p)), "el bucle");
            o.tros = OpcionsExport::Part; o.part = idPart;
            const auto q = projectePerExportar (p, o, {}, tros);
            comprova (prop (tros.first, segonsDeCompas (p, 4)) && prop (tros.second, segonsDeCompas (p, 6)), "la part (compassos 4-6)");
            comprova (! q.bucle.actiu, "sense el bucle");
        });

        prova ("0.83 Exportar: seguint l'estructura, amb cua i a 16 bits", [&]
        {
            Projecte p;
            p.clic = false;
            afegeixPista (p, "Veu", "audio");
            afegeixGravacio (p, 0, "Àudios/a.wav", "a", 4.0, 0.0, 0.0, 0.0);
            auto a = std::make_shared<AudioPresa>();
            a->fs = fs; a->canals = 1; a->longitud = (long long) (4 * fs);
            a->dades[0].assign ((size_t) a->longitud, 0.25f);
            MapaAudios mapa { { p.pistes[0].clips[0].presa, a } };
            auto pla = construeixPla (p, mapa);
            pla->ruta = { { 0.0, 1.0, {} }, { 2.0, 3.0, {} }, { 0.0, 0.5, {} } };   // 1 + 1 + 0,5 s
            comprova (prop (duradaExportacio (*pla, 0.0, 99.0), 2.5), "dura el que sumen els trams");
            const auto f = std::filesystem::temp_directory_path() / "mibase_083_ruta.wav";
            std::string error;
            comprova (exportaPla (*pla, fs, 0.0, 99.0, 16, 2.0, f, error), "s'exporta: " + error);
            const auto w = llegeixWav (f);
            comprova (w.correcte, "es llegeix");
            if (w.correcte)
            {
                const double d = (double) w.frames() / w.fs;
                std::printf ("        durada %.3f s\n", d);
                comprova (d > 2.5 && d < 2.6, "l'estructura, i la cua de silenci treta (queda 50 ms)");
                const auto mida = std::filesystem::file_size (f);
                comprova (std::abs ((double) mida - 44.0 - (double) w.frames() * 4.0) < 64.0, "16 bits (4 bytes per frame estèreo)");
                const float mig = w.mostres[(size_t) (2 * (long long) (1.5 * fs))];
                comprova (std::abs (mig - 0.25f) < 0.01f, "sona el segon tram");
            }
            std::filesystem::remove (f);
        });
    }
}
