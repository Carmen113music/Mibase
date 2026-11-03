#include "mibase/MotorEstudi.h"

#include <cmath>
#include <cstdlib>

namespace mibase
{
    //==========================================================================
    int slotDePista (const Pista& t) noexcept
    {
        int n = 0;
        for (char c : t.id)
            if (c >= '0' && c <= '9')
                n = (n * 10 + (c - '0')) % 100000;
        return n % maxSlots;
    }

    float guanyDb (double db) noexcept
    {
        return db <= -59.95 ? 0.0f : (float) std::pow (10.0, db / 20.0);
    }

    std::unique_ptr<Pla> construeixPla (const Projecte& p, const MapaAudios& audios)
    {
        auto pla = std::make_unique<Pla>();
        pla->tempo = p.tempo > 1.0 ? p.tempo : 120.0;
        pla->compasNum = std::max (1, p.compasNum);
        pla->clic = p.clic;
        pla->clicVolum = (float) p.clicVolum;
        pla->clicSo = p.clicSo;
        pla->master = guanyDb (p.masterDb);
        if (p.bucle.marcat && p.bucle.actiu && p.bucle.a > p.bucle.de)
        {
            const double spc = 60.0 / pla->tempo * pla->compasNum;
            pla->bucle = true;
            pla->bucleIni = p.bucle.de * spc;
            pla->bucleFi = p.bucle.a * spc;
        }

        const bool hiHaSolo = std::any_of (p.pistes.begin(), p.pistes.end(), [] (const Pista& t) { return t.solo; });
        for (const auto& t : p.pistes)
        {
            Pla::Pista pp;
            pp.slot = slotDePista (t);
            pp.canals = t.canals == 2 ? 2 : 1;
            pp.guany = guanyDb (t.faderDb);
            pp.pan = (float) std::clamp (t.pan, -1.0, 1.0);
            pp.sona = ! t.mut && (! hiHaSolo || t.solo);
            for (const auto& c : t.clips)
            {
                if (c.mut || c.presa.empty()) continue;
                auto it = audios.find (c.presa);
                if (it == audios.end() || it->second == nullptr || it->second->longitud <= 0) continue;
                Pla::Clip k;
                k.audio = it->second.get();
                k.pos = c.pos;
                k.inici = c.inici;
                k.fi = c.fi;
                k.fadeIn = std::max (0.0, c.fadeIn);
                k.fadeOut = std::max (0.0, c.fadeOut);
                k.guany = guanyDb (c.guanyDb);
                pp.clips.push_back (k);
                if (std::find (pla->audios.begin(), pla->audios.end(), it->second) == pla->audios.end())
                    pla->audios.push_back (it->second);
            }
            // Com a la pantalla: el clip de més amunt tapa els de sota (no se sumen).
            for (size_t k = 0; k < pp.clips.size(); ++k)
            {
                auto& c = pp.clips[k];
                std::vector<std::pair<double, double>> trams { { c.pos + c.inici, c.pos + c.fi } };
                for (size_t j = k + 1; j < pp.clips.size(); ++j)
                {
                    const double a = pp.clips[j].pos + pp.clips[j].inici, b = pp.clips[j].pos + pp.clips[j].fi;
                    std::vector<std::pair<double, double>> nous;
                    for (auto& t : trams)
                    {
                        if (b <= t.first || a >= t.second) { nous.push_back (t); continue; }
                        if (a > t.first) nous.push_back ({ t.first, a });
                        if (b < t.second) nous.push_back ({ b, t.second });
                    }
                    trams = std::move (nous);
                }
                c.trams = std::move (trams);
            }
            pla->pistes.push_back (std::move (pp));
        }
        return pla;
    }

    //==========================================================================
    void Mesclador::prepara (double nouFs, int maxBloc)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        pL.assign ((size_t) std::max (16, maxBloc), 0.0f);
        pR.assign ((size_t) std::max (16, maxBloc), 0.0f);
        clic.prepara (fs);
        reinicia();
    }

    void Mesclador::reinicia() noexcept
    {
        gFet.fill (false);
        masterFet = false;
        clic.reinicia();
    }

    void Mesclador::sonaClip (const Pla::Clip& c, int canalsPista, long long pos, int n) noexcept
    {
        for (const auto& t : c.trams)
            sonaTram (c, t.first, t.second, canalsPista, pos, n);
    }

    void Mesclador::sonaTram (const Pla::Clip& c, double de, double fins, int canalsPista, long long pos, int n) noexcept
    {
        const auto& a = *c.audio;
        const double durada = a.durada();
        const double ta = std::max (de, c.pos + std::max (c.inici, 0.0));
        const double tb = std::min (fins, c.pos + std::min (c.fi, durada));
        if (tb <= ta) return;

        const long long sA = (long long) std::ceil (ta * fs), sB = (long long) std::ceil (tb * fs);
        const int iA = (int) std::max (0LL, sA - pos);
        const int iB = (int) std::min ((long long) n, sB - pos);
        if (iB <= iA) return;

        const float* d0 = a.dades[0].data();
        const float* d1 = a.canals > 1 ? a.dades[1].data() : d0;
        const long long ultim = a.longitud - 1;

        for (int i = iA; i < iB; ++i)
        {
            const double ft = (double) (pos + i) / fs - c.pos;   // segons dins el fitxer
            double env = c.guany;
            if (c.fadeIn > 0.0 && ft < c.inici + c.fadeIn) env *= std::clamp ((ft - c.inici) / c.fadeIn, 0.0, 1.0);
            if (c.fadeOut > 0.0 && ft > c.fi - c.fadeOut) env *= std::clamp ((c.fi - ft) / c.fadeOut, 0.0, 1.0);

            const double x = ft * a.fs;   // mostra del fitxer (amb decimals); si la freqüència és diferent, s'interpola
            long long k = (long long) std::floor (x);
            const float frac = (float) (x - (double) k);
            if (k < 0) { k = 0; }
            if (k > ultim) k = ultim;
            const long long k2 = std::min (k + 1, ultim);
            const float l = d0[k] + (d0[k2] - d0[k]) * frac;
            const float r = d1[k] + (d1[k2] - d1[k]) * frac;

            if (canalsPista == 2)
            {
                pL[(size_t) i] += (float) env * l;
                pR[(size_t) i] += (float) env * r;
            }
            else
                pL[(size_t) i] += (float) env * (a.canals > 1 ? 0.5f * (l + r) : l);
        }
    }

    void Mesclador::processa (const Pla& pla, long long pos, int n, float* L, float* R, bool ambClic, Mesurador* vus) noexcept
    {
        if (n <= 0) return;
        n = std::min (n, (int) pL.size());

        for (const auto& t : pla.pistes)
        {
            const int s = t.slot % maxSlots;
            const float g = t.sona ? t.guany : 0.0f;
            const float objL = g * (t.pan <= 0.0f ? 1.0f : 1.0f - t.pan);
            const float objR = g * (t.pan >= 0.0f ? 1.0f : 1.0f + t.pan);
            if (! gFet[(size_t) s]) { gL[(size_t) s] = objL; gR[(size_t) s] = objR; gFet[(size_t) s] = true; }

            const float l0 = gL[(size_t) s], r0 = gR[(size_t) s];
            gL[(size_t) s] = objL;
            gR[(size_t) s] = objR;
            if (l0 < 1.0e-6f && r0 < 1.0e-6f && objL < 1.0e-6f && objR < 1.0e-6f)
                continue;   // no sona: ni es calcula

            std::fill (pL.begin(), pL.begin() + n, 0.0f);
            if (t.canals == 2) std::fill (pR.begin(), pR.begin() + n, 0.0f);
            for (const auto& c : t.clips)
                if (c.audio != nullptr)
                    sonaClip (c, t.canals, pos, n);

            const float* fontR = t.canals == 2 ? pR.data() : pL.data();
            const float dl = (objL - l0) / (float) n, dr = (objR - r0) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float a = pL[(size_t) i] * (l0 + dl * (float) i);
                const float b = fontR[i] * (r0 + dr * (float) i);
                pL[(size_t) i] = a;
                pR[(size_t) i] = b;
                L[i] += a;
                R[i] += b;
            }
            if (vus != nullptr)
            {
                vus[s * 2].processa (pL.data(), n);
                vus[s * 2 + 1].processa (pR.data(), n);
            }
        }

        // Volum de la sortida.
        if (! masterFet) { gMaster = pla.master; masterFet = true; }
        const float m0 = gMaster, dm = (pla.master - m0) / (float) n;
        gMaster = pla.master;
        if (std::abs (m0 - 1.0f) > 1.0e-6f || std::abs (dm) > 1.0e-9f)
            for (int i = 0; i < n; ++i)
            {
                const float g = m0 + dm * (float) i;
                L[i] *= g;
                R[i] *= g;
            }

        // Claqueta, enganxada als temps de la línia de temps.
        clic.setSo (pla.clicSo);
        clic.setVolum (pla.clicVolum);
        const double spb = fs * 60.0 / pla.tempo;
        std::fill (pL.begin(), pL.begin() + n, 0.0f);
        int fet = 0;
        for (long long k = (long long) std::floor ((double) pos / spb) - 1; ; ++k)
        {
            const long long s = (long long) std::llround ((double) k * spb);
            if (s < pos) continue;
            if (s >= pos + n) break;
            const int on = (int) std::max (0LL, s - pos);
            clic.processaSo (pL.data() + fet, on - fet);
            fet = on;
            if (ambClic)
            {
                const int num = std::max (1, pla.compasNum);
                clic.dispara (((k % num) + num) % num == 0);
            }
        }
        clic.processaSo (pL.data() + fet, n - fet);
        if (ambClic)
            for (int i = 0; i < n; ++i)
            {
                L[i] += pL[(size_t) i];
                R[i] += pL[(size_t) i];
            }
    }

    //==========================================================================
    MotorEstudi::MotorEstudi()
    {
        mapaEntrades.fill (-1);
        prepara (48000.0, 512);
    }

    MotorEstudi::~MotorEstudi()
    {
        grav.acaba();
        Pla* p = nullptr;
        while (nous.treu (p)) delete p;
        while (vells.treu (p)) delete p;
        delete pla;
    }

    void MotorEstudi::prepara (double nouFs, int nouMaxBloc)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        maxBloc = std::max (64, nouMaxBloc);
        mesclador.prepara (fs, maxBloc);
        mixL.assign ((size_t) maxBloc, 0.0f);
        mixR.assign ((size_t) maxBloc, 0.0f);
        estat.store ((int) Estat::Aturat, std::memory_order_release);
    }

    void MotorEstudi::setMapaEntrades (const std::vector<int>& mapa)
    {
        mapaEntrades.fill (-1);
        for (size_t i = 0; i < std::min (mapa.size(), mapaEntrades.size()); ++i)
            mapaEntrades[i] = mapa[i];
    }

    void MotorEstudi::publica (std::unique_ptr<Pla> nou)
    {
        recull();
        if (nou == nullptr) return;
        // Si n'hi ha de pendents, només compta l'últim.
        pendents.clear();
        if (! nous.posa (nou.get()))
        {
            pendents.push_back (std::move (nou));
            return;
        }
        nou.release();
    }

    void MotorEstudi::recull()
    {
        Pla* p = nullptr;
        while (vells.treu (p)) delete p;
        if (! pendents.empty() && nous.posa (pendents.back().get()))
        {
            pendents.back().release();
            pendents.clear();
        }
    }

    void MotorEstudi::reprodueix (double desDe) { cuaOrdres.posa ({ Ordre::Tipus::Reprodueix, desDe, 0.0, 0, 0 }); }
    void MotorEstudi::atura()                   { cuaOrdres.posa ({ Ordre::Tipus::Atura, 0.0, 0.0, 0, 0 }); }
    void MotorEstudi::situa (double t)          { cuaOrdres.posa ({ Ordre::Tipus::Situa, t, 0.0, 0, 0 }); }
    void MotorEstudi::grava (double desDe, double cursor, int entrada, int canals)
    {
        cuaOrdres.posa ({ Ordre::Tipus::Grava, desDe, cursor, entrada, canals });
    }

    void MotorEstudi::ordres() noexcept
    {
        Pla* p = nullptr;
        while (nous.treu (p))
        {
            if (pla != nullptr && ! vells.posa (pla)) { /* no hauria de passar mai: es perd (no s'esborra aquí) */ }
            pla = p;
        }

        Ordre o;
        while (cuaOrdres.treu (o))
        {
            switch (o.tipus)
            {
                case Ordre::Tipus::Reprodueix:
                    if ((Estat) estat.load() == Estat::Comptant || (Estat) estat.load() == Estat::Gravant) break;
                    pos = (long long) std::llround (o.t * fs);
                    compteFins = pos;
                    mesclador.reinicia();
                    estat.store ((int) Estat::Sonant, std::memory_order_release);
                    break;
                case Ordre::Tipus::Atura:
                    estat.store ((int) Estat::Aturat, std::memory_order_release);
                    break;
                case Ordre::Tipus::Situa:
                    if ((Estat) estat.load() == Estat::Sonant)
                    {
                        pos = (long long) std::llround (o.t * fs);
                        mesclador.reinicia();
                    }
                    break;
                case Ordre::Tipus::Grava:
                    pos = (long long) std::llround (o.t * fs);
                    compteFins = (long long) std::llround (o.t2 * fs);
                    entradaGrava = std::clamp (o.a, 0, maxEntrades - 1);
                    canalsGrava = o.b == 2 ? 2 : 1;
                    mesclador.reinicia();
                    estat.store ((int) (pos < compteFins ? Estat::Comptant : Estat::Gravant), std::memory_order_release);
                    break;
            }
        }
        posicio.store ((double) pos / fs, std::memory_order_relaxed);
    }

    void MotorEstudi::processa (const float* const* entrades, int numEntrades, float* const* sortides, int numSortides, int n) noexcept
    {
        ordres();

        // Vúmetres de les entrades (sempre, per veure si arriba senyal abans de gravar).
        for (int e = 0; e < maxEntrades; ++e)
        {
            const int k = mapaEntrades[(size_t) e];
            if (k >= 0 && k < numEntrades && entrades[k] != nullptr)
                vusEntrada[(size_t) e].processa (entrades[k], n);
        }

        auto ara = (Estat) estat.load (std::memory_order_relaxed);
        if (ara == Estat::Aturat || pla == nullptr)
            return;

        const bool gravant = ara == Estat::Comptant || ara == Estat::Gravant;
        int fet = 0;
        while (fet < n)
        {
            int m = std::min (n - fet, maxBloc);
            // El bucle (no mentre es grava): el tros acaba al final del bucle i torna a l'inici.
            const long long fiBucle = (long long) std::llround (pla->bucleFi * fs);
            const bool potTornar = pla->bucle && ! gravant && pos < fiBucle;
            if (potTornar && pos + m > fiBucle)
                m = (int) (fiBucle - pos);
            if (gravant && pos < compteFins && pos + m > compteFins)
                m = (int) (compteFins - pos);   // el canvi de «compte» a «grava», just al temps

            std::fill (mixL.begin(), mixL.begin() + m, 0.0f);
            std::fill (mixR.begin(), mixR.begin() + m, 0.0f);
            const bool ambClic = pla->clic || pos < compteFins;
            mesclador.processa (*pla, pos, m, mixL.data(), mixR.data(), ambClic, vus.data());

            if (gravant)
            {
                const float* canals[2] { nullptr, nullptr };
                for (int c = 0; c < canalsGrava; ++c)
                {
                    const int k = mapaEntrades[(size_t) std::min (entradaGrava + c, maxEntrades - 1)];
                    canals[c] = (k >= 0 && k < numEntrades && entrades[k] != nullptr) ? entrades[k] + fet : nullptr;
                }
                if (canalsGrava == 2 && canals[1] == nullptr) canals[1] = canals[0];
                grav.escriu (canals, m);
            }

            if (numSortides >= 2)
            {
                if (sortides[0] != nullptr) for (int i = 0; i < m; ++i) sortides[0][fet + i] = std::clamp (sortides[0][fet + i] + mixL[(size_t) i], -1.0f, 1.0f);
                if (sortides[1] != nullptr) for (int i = 0; i < m; ++i) sortides[1][fet + i] = std::clamp (sortides[1][fet + i] + mixR[(size_t) i], -1.0f, 1.0f);
            }
            else if (numSortides == 1 && sortides[0] != nullptr)
                for (int i = 0; i < m; ++i) sortides[0][fet + i] = std::clamp (sortides[0][fet + i] + 0.5f * (mixL[(size_t) i] + mixR[(size_t) i]), -1.0f, 1.0f);

            vusSortida[0].processa (mixL.data(), m);
            vusSortida[1].processa (mixR.data(), m);

            pos += m;
            fet += m;
            if (potTornar && pos >= fiBucle)
            {
                pos = (long long) std::llround (pla->bucleIni * fs);
                mesclador.reinicia();
            }
            if (ara == Estat::Comptant && pos >= compteFins)
            {
                ara = Estat::Gravant;
                estat.store ((int) ara, std::memory_order_release);
            }
        }
        posicio.store ((double) pos / fs, std::memory_order_relaxed);
    }

    //==========================================================================
    std::pair<double, double> trosPerExportar (const Projecte& p)
    {
        if (p.bucle.marcat && p.bucle.actiu && p.bucle.a > p.bucle.de)
        {
            const double spc = 60.0 / (p.tempo > 1.0 ? p.tempo : 120.0) * std::max (1, p.compasNum);
            return { p.bucle.de * spc, p.bucle.a * spc };
        }
        const bool hiHaSolo = std::any_of (p.pistes.begin(), p.pistes.end(), [] (const Pista& t) { return t.solo; });
        double a = 1.0e18, b = -1.0e18;
        for (const auto& t : p.pistes)
        {
            if (t.mut || (hiHaSolo && ! t.solo)) continue;
            for (const auto& c : t.clips)
            {
                if (c.mut || c.presa.empty()) continue;
                const Presa* s = nullptr;
                for (const auto& x : p.preses) if (x.id == c.presa) s = &x;
                if (s == nullptr) continue;
                const double ini = c.pos + std::max (c.inici, 0.0), fi = c.pos + std::min (c.fi, s->durada);
                if (fi <= ini) continue;
                a = std::min (a, ini);
                b = std::max (b, fi);
            }
        }
        if (b <= a) return { 0.0, 0.0 };
        return { std::max (0.0, a), b };
    }

    bool exportaMescla (const Pla& pla, double fs, double desDe, double finsA, const std::filesystem::path& desti, std::string& error,
                        const std::function<void (double)>& progres)
    {
        if (finsA <= desDe)
        {
            error = "No hi ha res que soni per exportar.";
            return false;
        }
        EscriptorWav wav;
        if (! wav.obre (desti, 2, fs, 24))
        {
            error = "No s'ha pogut crear el fitxer.";
            return false;
        }
        constexpr int bloc = 4096;
        Mesclador m;
        m.prepara (fs, bloc);
        std::vector<float> L (bloc), R (bloc), inter (bloc * 2);
        const long long ini = (long long) std::llround (desDe * fs), fi = (long long) std::llround (finsA * fs);
        for (long long p = ini; p < fi; p += bloc)
        {
            const int n = (int) std::min ((long long) bloc, fi - p);
            std::fill (L.begin(), L.end(), 0.0f);
            std::fill (R.begin(), R.end(), 0.0f);
            m.processa (pla, p, n, L.data(), R.data(), false, nullptr);
            for (int i = 0; i < n; ++i)
            {
                inter[(size_t) (2 * i)] = L[(size_t) i];
                inter[(size_t) (2 * i + 1)] = R[(size_t) i];
            }
            if (! wav.escriu (inter.data(), n))
            {
                error = "No s'ha pogut escriure al disc.";
                wav.tanca();
                return false;
            }
            if (progres) progres ((double) (p - ini) / (double) (fi - ini));
        }
        if (! wav.tanca())
        {
            error = "No s'ha pogut acabar el fitxer.";
            return false;
        }
        return true;
    }
}
