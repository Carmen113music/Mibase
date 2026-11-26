// 1.12: correccions de l'auditoria del Mastering Studio.
#include "mibase/Diagnosi.h"
#include "mibase/Dinamica.h"
#include "mibase/Equalitzador.h"
#include "mibase/Estereo.h"
#include "mibase/Limitador.h"
#include "mibase/Loudness.h"
#include "mibase/Multibanda.h"
#include "mibase/Rack.h"
namespace
{
    const mibase::rack::Preset& preset112 (const std::string& nom)
    {
        for (const auto& p : mibase::rack::presetsFabrica (mibase::rack::Tipus::Master)) if (p.nom == nom) return p;
        return mibase::rack::presetsFabrica (mibase::rack::Tipus::Master).front();
    }

    /** Fa sonar l'assistent de loudness fins que acaba (o s'acaba el so). */
    mibase::rack::AssistentLoudness::Estat assistent112 (mibase::rack::Rack& r, Estereo094& s)
    {
        mibase::rack::AssistentLoudness a;
        a.comenca (r, r.lufsObjectiu(), r.truePeakObjectiu());
        for (size_t o = 0, b = 0; o + 512 <= s.L.size() && a.actiu(); o += 512, ++b)
        {
            r.processa (s.L.data() + o, s.R.data() + o, 512);
            if (b % 10 == 0) a.actualitza (r);
        }
        return a.estat;
    }

    void proves112()
    {
        using namespace mibase;
        using namespace mibase::rack;

        prova ("1.12 Escoltes: mai a l'exportació (igualat, mono, solo, SC)", []
        {
            Rack r (Tipus::Master);
            r.posaEstat (preset112 ("Dens").estat);
            r.afegeix ("deessm");
            comprova (! escoltaEncesa (r), "d'entrada, cap escolta encesa");
            for (int i = 0; i < r.numModuls(); ++i)
            {
                auto* fx = r.modul (i)->fx.get();
                if (auto* l = dynamic_cast<limitador::Limitador*> (fx)) l->posa (limitador::Igualat, 1);
                if (auto* e = dynamic_cast<estereo::Estereo*> (fx)) e->posa (estereo::Mono, 1);
                if (auto* m = dynamic_cast<multibanda::Multibanda*> (fx)) m->posa (multibanda::index (1, multibanda::Solo), 1);
                if (auto* d = dynamic_cast<dinamica::Dinamica*> (fx)) d->posa (dinamica::Escolta, 1);
            }
            comprova (escoltaEncesa (r), "les escoltes enceses es detecten (per a l'avís)");
            apagaEscoltes (r);
            comprova (! escoltaEncesa (r), "apagaEscoltes les apaga totes, també dins del rack");
            limitador::Limitador sol;
            sol.posa (limitador::Igualat, 1);
            apagaEscoltes (sol);
            comprova (sol.valor (limitador::Igualat) < 0.5f, "i en un efecte sol");
        });

        prova ("1.12 Ajusta al perfil: també abaixa una mescla massa forta, i no diu «Fet» si no hi arriba", []
        {
            using E = AssistentLoudness::Estat;
            {
                Rack r (Tipus::Master);
                r.posaEstat (preset112 ("Màster net").estat);
                r.posaObjectiu (1);   // Streaming Standard, -14
                r.prepara (48000, 512);
                auto s = soroll094 (48000, 150, -9);
                const auto in = loudness::analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
                const auto e = assistent112 (r, s);
                auto t = soroll094 (48000, 20, -9, 777u);
                processa094 (r, t);
                const auto out = loudness::analitza (t.L.data(), t.R.data(), t.L.size(), 48000);
                std::printf ("        mix %.1f LUFS -> %.1f LUFS (Entrada %.1f dB, limitador %.1f dB)\n", in.integrated, out.integrated, r.entradaDb(), guanyLimitador (r));
                comprova (e == E::Fet && std::abs (out.integrated + 14.0f) < 0.8f && r.entradaDb() < 0.0f, "arriba a -14 amb l'Entrada del rack");
            }
            {
                Rack r (Tipus::Master);
                r.posaEstat (preset112 ("Només limitador").estat);
                r.posaObjectiu (1);
                r.prepara (48000, 512);
                auto* l = dynamic_cast<limitador::Limitador*> (r.modul (limitadorFinal (r))->fx.get());
                l->posa (limitador::Igualat, 1);
                auto s = soroll094 (48000, 150, -16);
                const auto e = assistent112 (r, s);
                auto t = soroll094 (48000, 20, -16, 778u);
                processa094 (r, t);
                const auto out = loudness::analitza (t.L.data(), t.R.data(), t.L.size(), 48000);
                comprova (l->valor (limitador::Igualat) < 0.5f && e == E::Fet && std::abs (out.integrated + 14.0f) < 0.8f,
                          "amb l'«Escolta igualat» encès, l'apaga i mesura el màster de debò (" + std::to_string (out.integrated) + ")");
            }
            {
                Rack r (Tipus::Master);
                r.posaEstat (preset112 ("Només limitador").estat);
                r.posaObjectiu (8);   // Broadcast, -23
                r.prepara (48000, 512);
                auto s = soroll094 (48000, 150, 6);   // molt per sobre: caldria més de -24 dB d'Entrada
                const auto e = assistent112 (r, s);
                comprova (e == E::NoConvergeix, "si no hi pot arribar, ho diu (no «Fet»)");
            }
        });

        prova ("1.12 Suggerència: amb una mescla més forta que el perfil, abaixa l'Entrada", []
        {
            auto s = soroll094 (48000, 60, -9);
            const auto an = diagnosi::analitza (s.L.data(), s.R.data(), s.L.size(), 48000);
            diagnosi::Context ctx; ctx.lufsObjectiu = -14; ctx.truePeakObjectiu = -1; ctx.perfil = "Streaming Standard";
            const auto sg = suggereixMaster (an, 6, ctx, 1);   // Transparent
            Rack r (Tipus::Master);
            r.posaEstat (sg.estat);
            r.prepara (48000, 512);
            auto t = soroll094 (48000, 30, -9, 99u);
            processa094 (r, t);
            const auto out = loudness::analitza (t.L.data(), t.R.data(), t.L.size(), 48000);
            std::printf ("        mix %.1f -> suggerència %.1f LUFS (Entrada %.1f dB)\n", an.loud.integrated, out.integrated, r.entradaDb());
            comprova (r.entradaDb() < -2.0f && out.integrated < -12.0f, "el màster queda a prop del perfil, no a -9");
        });

        prova ("1.12 Limitador: el true peak no passa del sostre (com a molt +0,15 dB)", []
        {
            float pitjor = -100.0f;
            for (int estil = 0; estil < limitador::numEstils; ++estil)
                for (float g : { 4.0f, 8.0f, 12.0f })
                {
                    limitador::Limitador l;
                    l.posa (limitador::EstilP, (float) estil); l.posa (limitador::Gain, g); l.posa (limitador::Ceiling, -1); l.posa (limitador::TruePeak, 1);
                    l.prepara (48000, 512);
                    auto s = soroll094 (48000, 10, -14, 1000u + (unsigned) estil);
                    processa094 (l, s);
                    const auto m = loudness::analitza (s.L.data() + 4800, s.R.data() + 4800, s.L.size() - 4800, 48000);
                    pitjor = std::max (pitjor, m.truePeakMax);
                }
            std::printf ("        pitjor true peak amb sostre -1: %.2f dBTP\n", pitjor);
            comprova (pitjor <= -0.85f, "tots els estils i guanys sota -0,85 dBTP");
        });

        prova ("1.12 Latència: el Mastering Studio amb Clipper no passa mai de 5 ms (Retard mínim)", []
        {
            for (double fs : { 44100.0, 48000.0, 88200.0, 96000.0 })
            {
                Rack r (Tipus::Master);
                r.posaEstat (preset112 ("Fort").estat);   // amb Clipper (12 mostres) i Limitador
                r.prepara (fs, 512);
                const int llindar = (int) (5 * 0.001 * fs);   // com el «Retard mínim» de 5 ms
                comprova (r.latencia() <= llindar, "a " + std::to_string ((int) fs) + " Hz: " + std::to_string (r.latencia()) + " ≤ " + std::to_string (llindar));
            }
        });

        prova ("1.12 Bypass: apagar i encendre el Limitador no fa clics ni canvia la latència", []
        {
            Rack r (Tipus::Master);
            r.posaEstat (preset112 ("Només limitador").estat);
            r.prepara (48000, 512);
            const int lim = limitadorFinal (r), lat = r.latencia();
            auto s = sinus094 (48000, 3, 220, -12);
            float salt[3] {}, prev = 0.0f;
            for (int b = 0; (b + 1) * 512 <= (int) s.L.size(); ++b)
            {
                if (b == 100) r.posaOn (lim, false);
                if (b == 200) r.posaOn (lim, true);
                float* L = s.L.data() + b * 512;
                r.processa (L, s.R.data() + b * 512, 512);
                const int z = b == 100 ? 1 : b == 200 ? 2 : 0;
                for (int i = 0; i < 512; ++i) { if (b > 20) salt[z] = std::max (salt[z], std::abs (L[i] - prev)); prev = L[i]; }
                if (b == 150) comprova (r.latencia() == lat, "apagat, la latència és la mateixa");
            }
            std::printf ("        salt normal %.4f, en apagar %.4f, en encendre %.4f\n", salt[0], salt[1], salt[2]);
            comprova (salt[1] < 1.5f * salt[0] && salt[2] < 1.5f * salt[0], "cap salt més gran que els normals");
        });

        prova ("1.12 Assistent i macros: aplicar una proposta no trepitja les bandes dels macros, i tocar el macro no la desfà", []
        {
            Rack r (Tipus::Master);
            r.posaEstat (preset112 ("Càlid").estat);
            auto* q = dynamic_cast<eq::Equalitzador*> (r.modul (0)->fx.get());
            const auto cos = q->banda (2);
            diagnosi::Accio a; a.tipus = diagnosi::Accio::EqCampana; a.freq = 250; a.guany = -2; a.q = 0.9f;
            diagnosi::aplica (r, a);
            const auto b2 = q->banda (2);
            comprova (b2.freq == cos.freq && b2.guany == cos.guany, "la banda «cos» (la del macro Escalfor) no es toca");
            int k = -1;
            for (int i = 6; i < eq::maxBandes; ++i) if (q->banda (i).usada && std::abs (q->banda (i).freq - 250.0f) < 1.0f) k = i;
            comprova (k >= 6 && std::abs (q->banda (k).guany + 2.0f) < 0.01f, "el tall va a una banda lliure");
            r.posaMacro (0, std::min (1.0f, r.macro (0) + 0.01f));
            comprova (k >= 0 && std::abs (q->banda (k).guany + 2.0f) < 0.01f && std::abs (q->banda (2).guany - cos.guany) < 0.2f, "tocar Escalfor no desfà el tall");
            diagnosi::Accio b; b.tipus = diagnosi::Accio::EqAire; b.freq = 12000; b.guany = 2;
            const float aire0 = q->banda (5).guany;
            diagnosi::aplica (r, b);
            const float aire1 = q->banda (5).guany;
            r.posaMacro (6, std::min (1.0f, r.macro (6) + 0.01f));
            const float aire2 = q->banda (5).guany;
            std::printf ("        aire: %.2f -> %.2f -> (macro +1 %%) %.2f dB\n", aire0, aire1, aire2);
            comprova (std::abs (aire1 - aire0 - 2.0f) < 0.01f && std::abs (aire2 - aire1) < 0.3f, "tocar el macro Aire mou poc a poc des d'on és (no torna enrere)");
        });

        prova ("1.12 posaEstatSuau: mateixa cadena, mateixos mòduls (A/B sense forats)", []
        {
            Rack r (Tipus::Master);
            r.posaEstat (preset112 ("Indie rock").estat);
            const auto abans = r.estat();
            auto* m0 = r.modul (0)->fx.get();
            diagnosi::Accio a; a.tipus = diagnosi::Accio::EqAire; a.freq = 12000; a.guany = 2;
            diagnosi::aplica (r, a);
            const auto despres = r.estat();
            r.posaEstatSuau (abans);
            comprova (r.estat() == abans && r.modul (0)->fx.get() == m0, "torna a l'estat d'abans amb els mateixos mòduls");
            r.posaEstatSuau (despres);
            comprova (r.estat() == despres && r.modul (0)->fx.get() == m0, "i al de després");
            r.posaEstatSuau (preset112 ("Fort").estat);   // una altra cadena: es refà
            comprova (r.estat() == preset112 ("Fort").estat, "si la cadena és diferent, la canvia sencera");
        });

        prova ("1.12 Analitza+: la corba de referència és relativa a 500 Hz–2 kHz (sense biaix)", []
        {
            float m = 0.0f;
            for (int b = 12; b <= 18; ++b) m += diagnosi::referencia (b);
            comprova (std::abs (m / 7.0f) < 0.01f, "mitjana de 500 Hz–2 kHz = 0");
        });

        prova ("1.12 Referència: la vella s'allibera quan el fil d'àudio ja no la fa servir", []
        {
            Rack r (Tipus::Master);
            r.posaEstat (preset112 ("Màster net").estat);
            r.prepara (48000, 512);
            auto a = std::make_shared<Referencia>(); a->fs = 48000; a->L.assign (48000, 0.1f); a->R = a->L;
            auto b = std::make_shared<Referencia>(); b->fs = 48000; b->L.assign (48000, 0.1f); b->R = b->L;
            r.posaReferencia (a);
            auto s = sinus094 (48000, 0.1, 440, -20);
            processa094 (r, s);
            r.posaReferencia (b);
            comprova (a.use_count() > 1, "just després de canviar-la, encara es guarda");
            auto t = sinus094 (48000, 0.1, 440, -20);
            processa094 (r, t);
            r.recull();
            comprova (a.use_count() == 1, "uns blocs després, ja no");
        });
    }
}
