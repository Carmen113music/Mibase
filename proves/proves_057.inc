// 0.57: gravar automatització (la línia «lliure» deixa manar el control) i mode desat.
namespace
{
    void proves057()
    {
        using namespace mibase;
        using namespace mibase::edicio;
        namespace au = mibase::automatitzacio;
        prova ("0.57 automatització: mentre es grava, mana el control (bandera lliure) i el mode es desa", []
        {
            const double fs = 48000.0;
            Projecte q;
            q.clic = false;
            const auto presa = afegeixPresa (q, "Àudios/a.wav", "a", 2.0).id;
            auto& t = afegeixPista (q, "A", "audio");
            afegeixClip (q, 0, presa, 0.0);
            t.faderDb = -6.0;
            auto& a = au::afegeix (q, t, au::volum, "Volum");
            au::posaPunt (a, { 0.0, 0.0, 0, 0 });
            a.grava = 1;
            auto au0 = std::make_shared<AudioPresa>();
            au0->fs = fs; au0->canals = 1; au0->longitud = (long long) (2 * fs);
            au0->dades[0].assign ((size_t) au0->longitud, 0.5f);
            MapaAudios m { { presa, au0 } };
            auto pla = construeixPla (q, m);
            Mesclador mx;
            mx.prepara (fs, 512);
            std::vector<float> L (512), R (512);
            mx.processa (*pla, 0, 512, L.data(), R.data(), false, nullptr, true);
            comprova (std::abs (L[500]) < 1e-6f, "llegint: silenci (la corba és a 0)");
            au::bandera (a.id)->store (true);
            for (int k = 0; k < 4; ++k) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); mx.processa (*pla, 512 * (k + 1), 512, L.data(), R.data(), false, nullptr, true); }
            comprova (std::abs (L[500] - 0.5f * guanyDb (-6.0)) < 1e-4f, "lliure: mana el fader (" + std::to_string (L[500]) + ")");
            au::bandera (a.id)->store (false);
            comprova (au::bandera (a.id) == au::bandera (a.id), "sempre la mateixa bandera");
            comprova (std::string (au::nomGrava (2)) == "Enganxa", "noms dels modes");
        });
    }
}
