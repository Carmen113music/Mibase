// 1.26: Mix Assistant: un sol EQ (i un sol efecte de cada mena) per pista, ordre fix, projectes de la 1.25 i historial.
namespace
{
    mx::Correccio correccioEq (float freq, float guany)
    {
        mx::Incidencia i;
        i.pista = "t1";
        i.proposta.tipus = mx::Proposta::Campana;
        i.proposta.freq = freq;
        i.proposta.guany = guany;
        i.proposta.q = 1.2f;
        return mx::preparaCorreccio (i);
    }
    int bandesUsades (const std::string& estat)
    {
        mibase::eq::Equalitzador e;
        e.posaEstat (estat);
        int n = 0;
        for (int k = 0; k < mibase::eq::maxBandes; ++k) n += e.banda (k).usada;
        return n;
    }

    void proves126()
    {
        using namespace mibase;
        prova ("1.26 Mix Assistant: tres correccions d'EQ, un sol EQ", []
        {
            Projecte p;
            edicio::afegeixPista (p, "Gtr", "audio");
            p.pistes[0].inserts.push_back ({ "i90", eq::idPlugin, "EQ Pro", true, "", {}, false });   // un EQ d'en Roger
            const auto a = mx::posaCorreccio (p, 0, correccioEq (300.0f, -3.0f), false);
            const auto b = mx::posaCorreccio (p, 0, correccioEq (1600.0f, -4.0f), false);
            const auto c = mx::posaCorreccio (p, 0, correccioEq (3200.0f, -2.0f), true);
            int mix = 0;
            for (const auto& x : p.pistes[0].inserts) mix += x.nom == mx::nomEqMix;
            comprova (mix == 1 && a.insert == b.insert && b.insert == c.insert, "un sol «EQ Pro · Mix Assistant»");
            comprova (a.banda != b.banda && b.banda != c.banda && a.banda >= 0, "una banda per correcció");
            comprova (p.pistes[0].inserts.size() == 2 && p.pistes[0].inserts[0].id == "i90", "l'EQ d'en Roger no es toca ni es mou");
            comprova (mx::estatCorreccio (p, a) == 0 && mx::estatCorreccio (p, c) == 1, "preparades apagades; la tercera, encesa");
            comprova (mx::encenCorreccio (p, a, true) && mx::estatCorreccio (p, a) == 1 && mx::estatCorreccio (p, b) == 0, "l'A/B encén només la seva banda");
            comprova (mx::treuCorreccio (p, b) && mx::estatCorreccio (p, b) == -1 && mx::estatCorreccio (p, a) == 1, "treure'n una no toca les altres");
            comprova (bandesUsades (p.pistes[0].inserts[1].estat) == 2, "queden dues bandes");
            mx::treuCorreccio (p, a);
            mx::treuCorreccio (p, c);
            comprova (p.pistes[0].inserts.size() == 1, "sense bandes, l'EQ del Mix Assistant se'n va");
        });

        prova ("1.26 Mix Assistant: compressor, de-esser i Estèreo, un de cada i en ordre", []
        {
            Projecte p;
            edicio::afegeixPista (p, "Veu", "audio");
            mx::Incidencia i;
            i.pista = "t1";
            i.proposta.tipus = mx::Proposta::Compressor;
            i.proposta.ratio = 3.0f; i.proposta.reduccioDb = 4.0f;
            const auto comp = mx::posaCorreccio (p, 0, mx::preparaCorreccio (i), false);
            i.proposta = {}; i.proposta.tipus = mx::Proposta::DeEsser; i.proposta.freq = 7000.0f;
            mx::posaCorreccio (p, 0, mx::preparaCorreccio (i), true);
            mx::posaCorreccio (p, 0, correccioEq (300.0f, -3.0f), true);
            i.proposta = {}; i.proposta.tipus = mx::Proposta::Compressor; i.proposta.ratio = 4.0f; i.proposta.reduccioDb = 6.0f;
            const auto comp2 = mx::posaCorreccio (p, 0, mx::preparaCorreccio (i), true);
            const auto& v = p.pistes[0].inserts;
            comprova (v.size() == 3 && comp.insert == comp2.insert, "el compressor nou reajusta el que ja hi era");
            comprova (v.size() == 3 && v[0].nom == mx::nomEqMix && v[1].nom == mx::nomCompressorMix && v[2].nom == mx::nomDeEsserMix,
                      "ordre: EQ, compressor, de-esser");
            dinamica::Dinamica d;
            d.posaEstat (v[1].estat);
            comprova (std::abs (d.valor (dinamica::Ratio) - 4.0f) < 0.01f && v[1].actiu, "amb els ajustos nous, encès");
        });

        prova ("1.26 Mix Assistant: els projectes de la 1.25 s'ajunten en un sol EQ", []
        {
            Projecte p;
            edicio::afegeixPista (p, "Gtr", "audio");
            auto& v = p.pistes[0].inserts;
            v.push_back ({ "i1", eq::idPlugin, "EQ Pro", true, "", {}, false });
            v.push_back ({ "i2", eq::idPlugin, mx::nomEqMix, true, correccioEq (300.0f, -3.0f).estat, {}, false });
            v.push_back ({ "i3", dinamica::idPlugin, "Dinàmica", true, "", {}, false });
            v.push_back ({ "i4", eq::idPlugin, std::string (mx::nomEqMix) + mx::sufixProva, false, correccioEq (1600.0f, -4.0f).estat, {}, false });
            comprova (mx::ajuntaEqsMix (p) == 2, "n'ajunta dos");
            const auto& w = p.pistes[0].inserts;
            comprova (w.size() == 3 && w[0].id == "i1" && w[1].id == "i3" && w[2].nom == mx::nomEqMix && w[2].actiu, "un sol EQ, al final");
            eq::Equalitzador e;
            e.posaEstat (w[2].estat);
            int actives = 0, apagades = 0;
            for (int k = 0; k < eq::maxBandes; ++k) if (e.banda (k).usada) (e.banda (k).activa ? actives : apagades)++;
            comprova (actives == 1 && apagades == 1, "la que estava aplicada, encesa; la prova, apagada");
            comprova (mx::ajuntaEqsMix (p) == 0, "un cop ajuntat, ja no hi ha res a fer");
        });

        prova ("1.26 Historial del Mix Assistant: es desa", []
        {
            Projecte p;
            edicio::afegeixPista (p, "Gtr", "audio");
            p.historialMix.push_back ({ "28/11 13:20", "t1", "Aplicada: EQ Pro, −5 dB a 1,6 kHz" });
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_126";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            comprova (disc::desa (dir / "C.mibase", p, "2026-11-28T13:00:00", "2026-11-28 130000", false).correcte, "es desa");
            const auto l = disc::llegeix (dir / "C.mibase");
            comprova (l && l->historialMix.size() == 1 && l->historialMix[0].pista == "t1" && l->historialMix[0].text.find ("1,6 kHz") != std::string::npos,
                      "i torna en obrir");
        });
    }
}
