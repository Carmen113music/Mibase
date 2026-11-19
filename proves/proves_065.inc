// 0.65: Mibase Delay; tonalitat amb la pista d'Acords i «Una altra».
#include "mibase/Delay.h"
namespace
{
    void proves065()
    {
        using namespace mibase;

        // Un clic (impuls) a l'inici i silenci; torna el so processat (2 canals) en blocs de 256.
        auto passa = [] (delay::Delay& d, double segons, std::vector<float>& L, std::vector<float>& R, float impuls = 0.5f)
        {
            const int n = (int) (48000 * segons);
            L.assign ((size_t) n, 0.0f); R.assign ((size_t) n, 0.0f);
            L[0] = R[0] = impuls;
            for (int i = 0; i < n; i += 256) d.processa (L.data() + i, R.data() + i, std::min (256, n - i));
        };
        auto pic = [] (const std::vector<float>& v, int a, int b)
        {
            float m = 0.0f;
            for (int i = std::max (0, a); i < std::min ((int) v.size(), b); ++i) m = std::max (m, std::fabs (v[(size_t) i]));
            return m;
        };
        auto finit = [] (const std::vector<float>& v) { return std::all_of (v.begin(), v.end(), [] (float x) { return std::isfinite (x); }); };

        prova ("0.65 Delay: l'eco arriba al temps que toca (ms i sincronitzat)", [&]
        {
            delay::Delay d;
            d.prepara (48000.0, 256);
            d.posa (delay::Sync, 0); d.posa (delay::Temps, 250); d.posa (delay::Feedback, 0); d.posa (delay::Mix, 50);
            std::vector<float> L, R;
            passa (d, 0.6, L, R);
            comprova (pic (L, 12000 - 20, 12000 + 20) > 0.2f, "eco a 250 ms");
            comprova (pic (L, 200, 11900) < 0.001f, "res abans");
            comprova (pic (L, 12100, 28000) < 0.001f, "amb feedback 0, un sol eco");
            // Sync: 1/4 a 120 bpm = 0,5 s
            std::atomic<double> tempo { 120.0 };
            delay::Delay s;
            s.posaTempo (&tempo);
            s.prepara (48000.0, 256);
            s.posa (delay::Sync, 1); s.posa (delay::Nota, 8); s.posa (delay::Feedback, 0); s.posa (delay::Mix, 50);
            comprova (std::string (delay::nomNota (8)) == "1/4", "la nota 8 és la negra");
            comprova (std::fabs (s.tempsL() - 0.5) < 1e-9, "1/4 a 120 = 0,5 s");
            passa (s, 0.8, L, R);
            comprova (pic (L, 24000 - 20, 24000 + 20) > 0.2f, "eco sincronitzat a 0,5 s");
        });

        prova ("0.65 Delay: feedback, ping-pong i mono", [&]
        {
            delay::Delay d;
            d.prepara (48000.0, 256);
            d.posa (delay::Sync, 0); d.posa (delay::Temps, 100); d.posa (delay::Feedback, 50); d.posa (delay::Mix, 50);
            std::vector<float> L, R;
            passa (d, 0.5, L, R);
            const float e1 = pic (L, 4800 - 10, 4800 + 10), e2 = pic (L, 9600 - 10, 9600 + 10), e3 = pic (L, 14400 - 10, 14400 + 10);
            comprova (e1 > 0.2f && std::fabs (e2 / e1 - 0.5f) < 0.05f && std::fabs (e3 / e2 - 0.5f) < 0.05f, "cada eco fa la meitat (feedback 50 %)");
            // ping-pong: el primer a l'esquerra, el segon a la dreta
            delay::Delay p;
            p.prepara (48000.0, 256);
            p.posa (delay::EncaminamentP, delay::PingPong); p.posa (delay::Sync, 0); p.posa (delay::Temps, 100); p.posa (delay::Feedback, 60);
            p.posa (delay::Mix, 100);
            passa (p, 0.4, L, R);
            comprova (pic (L, 4790, 4810) > 0.1f && pic (R, 4790, 4810) < 0.01f, "1r eco a l'esquerra");
            comprova (pic (R, 9590, 9610) > 0.05f && pic (L, 9590, 9610) < 0.01f, "2n eco a la dreta");
            const auto ec = p.ecos();
            comprova (ec.size() >= 2 && ec[0].l > 0.9f && ec[0].r == 0.0f && ec[1].r > 0.0f && ec[1].l == 0.0f, "els ecos dibuixats també alternen");
            // mono: tots dos costats iguals
            delay::Delay m;
            m.prepara (48000.0, 256);
            m.posa (delay::EncaminamentP, delay::Mono); m.posa (delay::Sync, 0); m.posa (delay::Temps, 100); m.posa (delay::Mix, 100);
            std::vector<float> l2 (24000, 0.0f), r2 (24000, 0.0f);
            l2[0] = 0.8f;   // només entra per l'esquerra
            for (int i = 0; i < 24000; i += 256) m.processa (l2.data() + i, r2.data() + i, std::min (256, 24000 - i));
            comprova (std::fabs (pic (l2, 4790, 4810) - pic (r2, 4790, 4810)) < 1e-4f && pic (r2, 4790, 4810) > 0.1f, "mono: l'eco surt pels dos costats");
        });

        prova ("0.65 Delay: estable amb feedback alt, freeze, tots els caràcters i canvis de temps", [&]
        {
            for (int car = 0; car < delay::numCaracters; ++car)
                for (int enc = 0; enc < delay::numEncaminaments; ++enc)
                {
                    delay::Delay d;
                    d.prepara (44100.0, 512);
                    d.posa (delay::CaracterP, (float) car); d.posa (delay::EncaminamentP, (float) enc); d.posa (delay::Feedback, 110);
                    d.posa (delay::Drive, 60); d.posa (delay::ModDepth, 100); d.posa (delay::Difusio, 100); d.posa (delay::Mix, 100);
                    d.posa (delay::Sync, 0); d.posa (delay::Temps, 80);
                    std::vector<float> L (44100 * 3), R (44100 * 3);
                    for (size_t i = 0; i < L.size(); ++i) { L[i] = 0.5f * std::sin ((float) i * 0.05f); R[i] = 0.5f * std::sin ((float) i * 0.031f); }
                    for (int i = 0; i < (int) L.size(); i += 512)
                    {
                        if (i == 44100) d.posa (delay::Temps, 700);      // canvi de temps a mig so
                        if (i == 88200) d.posa (delay::Temps, 3);
                        d.processa (L.data() + i, R.data() + i, std::min (512, (int) L.size() - i));
                    }
                    comprova (finit (L) && finit (R) && pic (L, 0, (int) L.size()) < 6.0f && pic (R, 0, (int) R.size()) < 6.0f,
                              std::string ("estable: ") + delay::nomCaracter (car) + " / " + delay::nomEncaminament (enc));
                }
            // freeze: el que sona es queda; el que entra després no hi entra
            delay::Delay f;
            f.prepara (48000.0, 256);
            f.posa (delay::Sync, 0); f.posa (delay::Temps, 100); f.posa (delay::Feedback, 30); f.posa (delay::Mix, 100);
            std::vector<float> L (48000, 0.0f), R (48000, 0.0f);
            L[0] = R[0] = 0.5f;
            for (int i = 0; i < 48000; i += 256)
            {
                if (i == 256) f.posa (delay::Freeze, 1);
                if (i == 24064) { L[(size_t) i] = R[(size_t) i] = 0.9f; }   // un cop fort amb el freeze encès
                f.processa (L.data() + i, R.data() + i, std::min (256, 48000 - i));
            }
            comprova (pic (L, 43000, 48000) > 0.3f, "amb freeze, l'eco continua sense apagar-se");
            comprova (pic (L, 24064 + 4790, 24064 + 4810) < 0.6f, "el cop nou no entra als ecos");
            // buidar
            f.buida();
            std::vector<float> a (9600, 0.0f), b (9600, 0.0f);
            f.posa (delay::Freeze, 0);
            for (int i = 0; i < 9600; i += 256) f.processa (a.data() + i, b.data() + i, std::min (256, 9600 - i));
            comprova (pic (a, 0, 9600) < 1e-6f, "buida() esborra els ecos");
        });

        prova ("0.65 Delay: filtres, ducking, estat i presets", [&]
        {
            // high cut: un eco amb aguts surt més fluix que sense filtre
            auto ecoAguts = [&] (float hc)
            {
                delay::Delay d;
                d.prepara (48000.0, 256);
                d.posa (delay::Sync, 0); d.posa (delay::Temps, 100); d.posa (delay::Feedback, 70); d.posa (delay::HighCut, hc); d.posa (delay::Mix, 100);
                std::vector<float> L (48000), R (48000);
                for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = i < 4800 ? 0.3f * std::sin ((float) i * 2.0f * 3.14159f * 8000.0f / 48000.0f) : 0.0f;
                for (int i = 0; i < 48000; i += 256) d.processa (L.data() + i, R.data() + i, std::min (256, 48000 - i));
                return pic (L, 4800 * 4, 4800 * 5);
            };
            comprova (ecoAguts (2000.0f) < 0.3f * ecoAguts (20000.0f), "el High Cut enfosqueix les repeticions");
            // ducking: mentre sona fort, els ecos baixen
            auto ecoMentreToca = [&] (float duck)
            {
                delay::Delay d;
                d.prepara (48000.0, 256);
                d.posa (delay::Sync, 0); d.posa (delay::Temps, 200); d.posa (delay::Feedback, 0); d.posa (delay::Mix, 100);
                d.posa (delay::DuckAmt, duck); d.posa (delay::DuckThr, -30);
                std::vector<float> L (48000), R (48000);
                for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = 0.5f * std::sin ((float) i * 0.03f);
                for (int i = 0; i < 48000; i += 256) d.processa (L.data() + i, R.data() + i, std::min (256, 48000 - i));
                return pic (L, 30000, 48000);
            };
            const float sense = ecoMentreToca (0.0f), amb = ecoMentreToca (20.0f);
            comprova (amb < sense * 0.2f, "amb ducking de 20 dB els ecos baixen mentre toques");
            // estat
            delay::Delay d;
            d.posa (delay::CaracterP, delay::Cinta); d.posa (delay::Temps, 432); d.posa (delay::Feedback, 61); d.posa (delay::Nota, 3);
            delay::Delay e;
            e.posaEstat (d.estat());
            comprova (e.estat() == d.estat() && (int) e.valor (delay::CaracterP) == delay::Cinta, "l'estat es desa i es llegeix igual");
            e.posaEstat ("mibase-dinamica=1;mode=2");
            comprova (e.estat() == d.estat(), "un estat d'un altre efecte no hi entra");
            comprova (delay::textValor (delay::HighCut, 20000) == "Off" && delay::textValor (delay::Feedback, 35) == "35 %", "textos dels valors");
            // presets
            const auto& v = delay::presetsFabrica();
            std::printf ("        (%zu presets)\n", v.size());
            std::set<std::string> noms;
            bool be = true;
            for (const auto& p : v)
            {
                noms.insert (p.nom);
                delay::Delay x;
                x.posaEstat (p.estat);
                be = be && x.estat() == p.estat && ! p.descripcio.empty()
                     && std::find (delay::categories().begin(), delay::categories().end(), p.categoria) != delay::categories().end();
            }
            comprova (v.size() >= 24 && noms.size() == v.size() && be, "presets vàlids, sense noms repetits i amb categoria");
            // biblioteca de l'usuari
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_065";
            std::filesystem::remove_all (dir);
            delay::BibliotecaPresets b;
            b.desaPreset ({ "Meu", "Meus", "", d.estat() });
            b.commutaFavorit (delay::BibliotecaPresets::clauUsuari ("Meu"));
            comprova (b.desa (dir / "presets.json"), "desa la biblioteca");
            delay::BibliotecaPresets c;
            comprova (c.carrega (dir / "presets.json") && c.usuari.size() == 1 && c.esFavorit ("u:Meu"), "la torna a llegir");
            std::filesystem::remove_all (dir);
        });

        prova ("0.65 Tonalitat: la pista d'Acords compta i «Una altra» la canvia", [&]
        {
            using namespace mibase::edicio;
            Projecte q;
            q.tempo = 120.0;
            afegeixPista (q, "Acords", "acords");
            const int A = 0;
            comprova (esAcords (q.pistes[(size_t) A]), "pista d'Acords");
            auto acord = [&] (const char* nom, double a, double b) { Clip k; k.acord = nom; k.pos = a; k.inici = 0.0; k.fi = b - a; q.pistes[(size_t) A].clips.push_back (k); };
            auto d = harmonia::detecta (q);
            comprova (! d.usable && d.acords == 0, "sense acords, res");
            acord ("Am", 0.0, 2.0);
            acord ("Dm", 2.0, 4.0);
            acord ("E", 4.0, 6.0);
            acord ("Am", 6.0, 8.0);
            d = harmonia::detecta (q);
            comprova (d.acords == 4 && d.usable, "amb acords ja n'hi ha una");
            comprova (d.tonalitat.to == 9 && d.tonalitat.escala == 1, "Am Dm E Am → La menor");
            comprova (harmonia::tonalitatEfectiva (q) == d.tonalitat, "és la que fa servir tot el programa");
            // pocs acords: dubtosa, amb diverses candidates
            Projecte r;
            afegeixPista (r, "Acords", "acords");
            Clip k; k.acord = "C"; k.pos = 0.0; k.fi = 2.0; r.pistes[0].clips.push_back (k);
            k.acord = "G"; k.pos = 2.0; r.pistes[0].clips.push_back (k);
            auto e = harmonia::detecta (r);
            comprova (e.usable && e.dubtosa && e.candidats.size() >= 2, "C G: dubtosa i amb candidates");
            const auto primera = harmonia::tonalitatEfectiva (r);
            r.tePreferida = true;
            r.tonalitatPreferida = harmonia::seguentCandidata (r);
            comprova (! (r.tonalitatPreferida == primera) && harmonia::tonalitatEfectiva (r) == r.tonalitatPreferida, "«Una altra» en tria una altra");
            // en donar voltes, torna a la primera
            std::set<std::pair<int, int>> vistes { { primera.to, primera.escala } };
            for (int i = 0; i < 6; ++i)
            {
                r.tonalitatPreferida = harmonia::seguentCandidata (r);
                vistes.insert ({ r.tonalitatPreferida.to, r.tonalitatPreferida.escala });
            }
            comprova (vistes.size() == harmonia::detecta (r).candidats.size(), "passa per totes les candidates");
            // una preferida que ja no encaixa: mana la més probable
            q.tePreferida = true;
            q.tonalitatPreferida = { 6, 0 };   // F# major: no encaixa amb Am Dm E Am
            comprova (harmonia::tonalitatEfectiva (q).to == 9, "la preferida que no encaixa no mana");
            // es desa
            const auto dir = std::filesystem::temp_directory_path() / "mibase_proves_065b";
            std::filesystem::remove_all (dir);
            std::filesystem::create_directories (dir);
            r.tonalitatPreferida = { 7, 0 };
            comprova (disc::desa (dir / "p.mibase", r, "2026-11-19T12:00:00", "x", false).correcte, "desat");
            const auto l = disc::llegeix (dir / "p.mibase");
            comprova (l.has_value() && l->tePreferida && l->tonalitatPreferida == Tonalitat { 7, 0 }, "la preferida es torna a llegir");
            std::filesystem::remove_all (dir);
        });
    }
}
