#include "mibase/MotorEstudi.h"
#include "mibase/Midi.h"
#include "mibase/Edicio.h"

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

    namespace
    {
        /** Trossos de [a, b) que no tapen els intervals de «sobre». */
        std::vector<std::pair<double, double>> destapat (double a, double b, const std::vector<std::pair<double, double>>& sobre)
        {
            std::vector<std::pair<double, double>> trams { { a, b } };
            for (auto& [x, y] : sobre)
            {
                std::vector<std::pair<double, double>> nous;
                for (auto& t : trams)
                {
                    if (y <= t.first || x >= t.second) { nous.push_back (t); continue; }
                    if (x > t.first) nous.push_back ({ t.first, x });
                    if (y < t.second) nous.push_back ({ y, t.second });
                }
                trams = std::move (nous);
            }
            return trams;
        }

        void notesDePista (const Projecte& p, const Pista& t, Pla::Pista& pp)
        {
            // 0.28: amb el mapa de tempo, un tick del clip va al moment que li toca de la cançó
            const bool mapa = edicio::teCanvisTempo (p);
            const double spt = midi::segonsPerTick (p);
            std::vector<const Clip*> clips;
            for (const auto& c : t.clips)
                if (c.midi && ! c.mut) clips.push_back (&c);

            for (size_t k = 0; k < clips.size(); ++k)
            {
                const auto& c = *clips[k];
                // Com a l'àudio: el clip de més amunt de la llista tapa els de sota.
                std::vector<std::pair<double, double>> sobre;
                for (size_t j = k + 1; j < clips.size(); ++j)
                    sobre.push_back ({ clips[j]->iniciVisible(), clips[j]->fiVisible() });
                const auto trams = destapat (c.iniciVisible(), c.fiVisible(), sobre);
                auto tram = [&trams] (double t) -> const std::pair<double, double>*
                {
                    for (auto& x : trams) if (t >= x.first - 1e-9 && t < x.second - 1e-9) return &x;
                    return nullptr;
                };

                const double tick0 = mapa ? midi::ticks (p, c.pos) : 0.0;
                auto moment = [&] (double tickRel) { return mapa ? midi::segons (p, tick0 + tickRel) : c.pos + spt * tickRel; };
                for (const auto& n : c.notes)
                {
                    const double ini = moment ((double) n.ini);
                    const auto* tr = tram (ini);
                    if (tr == nullptr) continue;
                    Pla::Nota pn;
                    pn.ini = std::max (ini, tr->first);
                    pn.fi = std::min (moment ((double) (n.ini + std::max (1LL, n.dur))), tr->second);
                    if (pn.fi <= pn.ini) pn.fi = pn.ini + 0.001;
                    pn.to = (std::uint8_t) std::clamp (n.to, 0, 127);
                    pn.vel = (std::uint8_t) std::clamp (n.vel, 1, 127);
                    pn.canal = (std::uint8_t) std::clamp (n.canal, 0, 15);
                    pp.notes.push_back (pn);
                }
                for (const auto& e : c.events)
                {
                    const double te = moment ((double) e.tick);
                    if (tram (te) == nullptr) continue;
                    Pla::Event pe;
                    pe.t = te;
                    const int canal = std::clamp (e.canal, 0, 15);
                    const int tipus = e.tipus & 0xF0;
                    pe.estat = (std::uint8_t) (tipus | canal);
                    switch (tipus)
                    {
                        case 0xE0: { const int v = std::clamp (e.valor, 0, 16383); pe.dada1 = (std::uint8_t) (v & 127); pe.dada2 = (std::uint8_t) (v >> 7); break; }
                        case 0xD0: case 0xC0: pe.dada1 = (std::uint8_t) std::clamp (e.valor, 0, 127); break;
                        case 0xB0: case 0xA0: pe.dada1 = (std::uint8_t) std::clamp (e.num, 0, 127); pe.dada2 = (std::uint8_t) std::clamp (e.valor, 0, 127); break;
                        default: continue;
                    }
                    pp.events.push_back (pe);
                }
            }
            std::stable_sort (pp.notes.begin(), pp.notes.end(), [] (const Pla::Nota& a, const Pla::Nota& b) { return a.ini < b.ini; });
            std::stable_sort (pp.events.begin(), pp.events.end(), [] (const Pla::Event& a, const Pla::Event& b) { return a.t < b.t; });
        }
    }

    std::unique_ptr<Pla> construeixPla (const Projecte& p, const MapaAudios& audios, const ProveidorInstruments& instruments,
                                        const ProveidorEfectes& efectes, const MapaAudios* corregides)
    {
        auto pla = std::make_unique<Pla>();
        pla->tempo = p.tempo > 1.0 ? p.tempo : 120.0;
        if (edicio::teCanvisTempo (p)) pla->mapaTempo = edicio::mapaTempo (p);
        pla->compasNum = std::max (1, p.compasNum);
        pla->clic = p.clic;
        pla->clicVolum = (float) p.clicVolum;
        pla->clicSo = p.clicSo;
        pla->master = guanyDb (p.masterDb);
        pla->sortidaMaster = std::clamp (p.sortidaMaster, 0, 31);
        if (efectes)
            for (const auto& x : p.insertsMaster)
                if (x.actiu)
                    if (auto e = efectes (x)) pla->insertsMaster.push_back (std::move (e));
        if (const auto* es = edicio::estructuraActiva (p))
            for (auto& pc : es->peces)
                for (auto& x : p.parts)
                    if (x.id == pc.part && x.a > x.de)
                    {
                        Pla::Tram tr { edicio::segonsDeCompas (p, x.de), edicio::segonsDeCompas (p, x.a), {} };
                        for (auto& id : pc.silencis)
                            for (auto& t : p.pistes)
                                if (t.id == id) tr.silenci.set ((size_t) (slotDePista (t) % maxSlots));
                        pla->ruta.push_back (tr);
                    }
        if (p.bucle.marcat && p.bucle.actiu && p.bucle.a > p.bucle.de && pla->ruta.empty())
        {
            pla->bucle = true;
            pla->bucleIni = edicio::iniciBucle (p);
            pla->bucleFi = edicio::fiBucle (p);
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
            const bool ambVeu = corregides != nullptr && ! esMidi (t) && veuActiva (t);
            for (const auto& c : t.clips)
            {
                if (c.mut || c.presa.empty()) continue;
                const std::shared_ptr<const AudioPresa>* au = nullptr;
                if (auto it0 = audios.find (c.presa); it0 != audios.end()) au = &it0->second;
                if (ambVeu)
                    if (auto ic = corregides->find (c.presa); ic != corregides->end() && ic->second != nullptr && ic->second->longitud > 0) au = &ic->second;
                if (au == nullptr || *au == nullptr || (*au)->longitud <= 0) continue;
                const auto audio = *au;
                Pla::Clip k;
                k.audio = audio.get();
                k.pos = c.pos;
                k.inici = c.inici;
                k.fi = c.fi;
                k.fadeIn = std::max (0.0, c.fadeIn);
                k.fadeOut = std::max (0.0, c.fadeOut);
                k.guany = guanyDb (c.guanyDb);
                pp.clips.push_back (k);
                if (std::find (pla->audios.begin(), pla->audios.end(), audio) == pla->audios.end())
                    pla->audios.push_back (audio);
            }
            // Com a la pantalla: el clip de més amunt tapa els de sota (no se sumen).
            for (size_t k = 0; k < pp.clips.size(); ++k)
            {
                auto& c = pp.clips[k];
                std::vector<std::pair<double, double>> trams { { c.pos + c.inici, c.pos + c.fi } };
                for (size_t j = k + 1; j < pp.clips.size(); ++j)
                {
                    // 0.14: durant els fades del de dalt (crossfade), el de sota també sona.
                    const double a = pp.clips[j].pos + pp.clips[j].inici + pp.clips[j].fadeIn,
                                 b = pp.clips[j].pos + pp.clips[j].fi - pp.clips[j].fadeOut;
                    if (b <= a) continue;
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
            if (esMidi (t))
            {
                pp.canals = 2;
                pp.clips.clear();
                if (instruments) pp.instrument = instruments (t);
                if (pp.instrument != nullptr)
                    notesDePista (p, t, pp);
            }
            if (efectes)
                for (const auto& x : t.inserts)
                    if (x.actiu && ! esInsertVeu (x))
                        if (auto e = efectes (x)) pp.inserts.push_back (std::move (e));
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
        if (slots.size() != (size_t) maxSlots) slots.assign ((size_t) maxSlots, EstatSlot {});
        evs.assign ((size_t) maxEventsBloc, EventInstrument {});
        reinicia();
    }

    void Mesclador::reinicia() noexcept
    {
        gFet.fill (false);
        masterFet = false;
        clic.reinicia();
        apagaNotes();
    }

    void Mesclador::apagaNotes() noexcept
    {
        for (auto& s : slots)
            if (s.inst != nullptr) s.apagar = true;
    }

    void Mesclador::canviDePla (const Pla* vell, const Pla* nou) noexcept
    {
        if (vell == nullptr || slots.empty()) return;
        for (const auto& t : vell->pistes)
        {
            if (t.instrument == nullptr) continue;
            bool continua = false;
            if (nou != nullptr)
                for (const auto& u : nou->pistes)
                    if (u.slot == t.slot && u.instrument == t.instrument) { continua = true; break; }
            if (continua) continue;
            // Ja no sonarà: s'apaga ara (l'objecte encara és viu, el pla vell el manté) i el slot s'oblida.
            t.instrument->apagaTot();
            auto& es = slots[(size_t) (t.slot % maxSlots)];
            if (es.inst == t.instrument.get()) { es.inst = nullptr; es.n = 0; es.apagar = false; }
        }
    }

    void Mesclador::afegeixEvent (int offset, std::uint8_t estat, std::uint8_t d1, std::uint8_t d2) noexcept
    {
        if (numEvs >= (int) evs.size()) return;
        // Ordenat per offset (inserció: n'hi ha pocs); al mateix offset, en l'ordre d'arribada.
        int i = numEvs++;
        while (i > 0 && evs[(size_t) (i - 1)].offset > offset) { evs[(size_t) i] = evs[(size_t) (i - 1)]; --i; }
        evs[(size_t) i] = { offset, estat, d1, d2 };
    }

    void Mesclador::instrumentPista (const Pla::Pista& t, long long pos, int n, bool reproduint, const EventDirecte* directe, int numDirecte) noexcept
    {
        auto& es = slots[(size_t) (t.slot % maxSlots)];
        auto* inst = t.instrument.get();
        numEvs = 0;

        if (es.inst != inst)
        {
            es.inst = inst;
            es.n = 0;
            es.apagar = false;
        }
        if (es.apagar)
        {
            es.apagar = false;
            es.n = 0;
            inst->apagaTot();
        }

        // Notes que s'acaben dins aquest bloc.
        for (int k = 0; k < es.n;)
        {
            auto& a = es.act[(size_t) k];
            if (a.fi < pos + n)
            {
                afegeixEvent ((int) std::clamp (a.fi - pos, 0LL, (long long) n - 1), (std::uint8_t) (0x80 | a.canal), a.to, 0);
                es.act[(size_t) k] = es.act[(size_t) (es.n - 1)];
                --es.n;
            }
            else ++k;
        }

        // En directe (piano roll, teclat): al començament del bloc.
        for (int k = 0; k < numDirecte; ++k)
            if (directe[k].slot == t.slot)
                afegeixEvent (0, directe[k].estat, directe[k].dada1, directe[k].dada2);

        if (reproduint)
        {
            // Controladors i altres missatges.
            {
                auto it = std::lower_bound (t.events.begin(), t.events.end(), pos, [this] (const Pla::Event& e, long long p)
                                            { return std::llround (e.t * fs) < p; });
                for (; it != t.events.end(); ++it)
                {
                    const long long s = std::llround (it->t * fs);
                    if (s >= pos + n) break;
                    afegeixEvent ((int) (s - pos), it->estat, it->dada1, it->dada2);
                }
            }
            // Notes que comencen dins aquest bloc (si la pista no està silenciada).
            if (t.sona && ! silenciAra[(size_t) (t.slot % maxSlots)])
            {
                auto it = std::lower_bound (t.notes.begin(), t.notes.end(), pos, [this] (const Pla::Nota& x, long long p)
                                            { return std::llround (x.ini * fs) < p; });
                for (; it != t.notes.end(); ++it)
                {
                    const long long s = std::llround (it->ini * fs);
                    if (s >= pos + n) break;
                    const int off = (int) (s - pos);
                    const long long fi = std::max (s + 1, std::llround (it->fi * fs));
                    // La mateixa nota ja sona: s'apaga abans de tornar-la a tocar.
                    bool ja = false;
                    for (int k = 0; k < es.n; ++k)
                    {
                        auto& a = es.act[(size_t) k];
                        if (a.to == it->to && a.canal == it->canal)
                        {
                            afegeixEvent (off, (std::uint8_t) (0x80 | a.canal), a.to, 0);
                            a.fi = fi;
                            ja = true;
                            break;
                        }
                    }
                    if (! ja)
                    {
                        if (es.n >= maxNotesActives) continue;   // massa notes alhora: aquesta no sona
                        es.act[(size_t) es.n++] = { fi, it->to, it->canal };
                    }
                    afegeixEvent (off, (std::uint8_t) (0x90 | it->canal), it->to, it->vel);
                    // Una nota que acaba dins el mateix bloc on comença.
                    if (fi < pos + n)
                    {
                        for (int k = 0; k < es.n; ++k)
                            if (es.act[(size_t) k].to == it->to && es.act[(size_t) k].canal == it->canal)
                            {
                                es.act[(size_t) k] = es.act[(size_t) (es.n - 1)];
                                --es.n;
                                break;
                            }
                        afegeixEvent ((int) (fi - pos), (std::uint8_t) (0x80 | it->canal), it->to, 0);
                    }
                }
            }
        }
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

    void Mesclador::processa (const Pla& pla, long long pos, int n, float* L, float* R, bool ambClic, Mesurador* vus,
                              bool reproduint, const EventDirecte* directe, int numDirecte) noexcept
    {
        if (n <= 0) return;
        n = std::min (n, (int) pL.size());

        for (const auto& t : pla.pistes)
        {
            const int s = t.slot % maxSlots;
            bool instrumentFet = false;
            if (t.instrument != nullptr && ! slots.empty())
            {
                // L'instrument sempre rep els seus missatges (també silenciat: així les notes s'apaguen).
                instrumentPista (t, pos, n, reproduint, directe, numDirecte);
                if (numEvs == 0 && ! t.instrument->fentSo())
                    continue;
                t.instrument->processa (evs.data(), numEvs, pL.data(), pR.data(), n);
                instrumentFet = true;
            }
            else if (! reproduint)
                continue;   // aturat: només sonen els instruments

            const float g = t.sona && ! silenciAra[(size_t) s] ? t.guany : 0.0f;
            const float objL = g * (t.pan <= 0.0f ? 1.0f : 1.0f - t.pan);
            const float objR = g * (t.pan >= 0.0f ? 1.0f : 1.0f + t.pan);
            if (! gFet[(size_t) s]) { gL[(size_t) s] = objL; gR[(size_t) s] = objR; gFet[(size_t) s] = true; }

            const float l0 = gL[(size_t) s], r0 = gR[(size_t) s];
            gL[(size_t) s] = objL;
            gR[(size_t) s] = objR;
            if (l0 < 1.0e-6f && r0 < 1.0e-6f && objL < 1.0e-6f && objR < 1.0e-6f)
                continue;   // no sona: ni es calcula

            if (! instrumentFet)
            {
                std::fill (pL.begin(), pL.begin() + n, 0.0f);
                if (t.canals == 2) std::fill (pR.begin(), pR.begin() + n, 0.0f);
                for (const auto& c : t.clips)
                    if (c.audio != nullptr)
                        sonaClip (c, t.canals, pos, n);
            }

            bool estereo = t.canals == 2 || instrumentFet;
            if (! t.inserts.empty())
            {
                // 0.19: efectes de la pista (abans del fader). Una pista mono hi entra pels dos costats.
                if (! estereo) std::copy (pL.begin(), pL.begin() + n, pR.begin());
                estereo = true;
                for (const auto& e : t.inserts) e->processa (pL.data(), pR.data(), n);
            }
            const float* fontR = estereo ? pR.data() : pL.data();
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

        // 0.19: cadena de màstering (abans del volum del màster).
        for (const auto& e : pla.insertsMaster) e->processa (L, R, n);

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

        if (! reproduint) return;

        // Claqueta, enganxada als temps de la línia de temps.
        clic.setSo (pla.clicSo);
        clic.setVolum (pla.clicVolum);
        std::fill (pL.begin(), pL.begin() + n, 0.0f);
        int fet = 0;
        if (pla.mapaTempo.size() > 1)
        {
            // 0.28: amb canvis de tempo per parts, tram a tram (el primer temps de cada compàs, el fort)
            for (size_t ti = 0; ti < pla.mapaTempo.size(); ++ti)
            {
                const auto& tr = pla.mapaTempo[ti];
                const double t0 = tr.segons * fs, t1 = ti + 1 < pla.mapaTempo.size() ? pla.mapaTempo[ti + 1].segons * fs : 1.0e300;
                if (t1 <= (double) pos || t0 >= (double) (pos + n)) continue;
                const double spbT = fs * 60.0 / tr.tempo;
                for (long long k = std::max (0LL, (long long) std::floor (((double) pos - t0) / spbT) - 1); ; ++k)
                {
                    const double sd = t0 + (double) k * spbT;
                    if (sd >= t1 - 0.5) break;
                    const long long s = (long long) std::llround (sd);
                    if (s < pos) continue;
                    if (s >= pos + n) break;
                    const int on = (int) std::max (0LL, s - pos);
                    if (on < fet) continue;
                    clic.processaSo (pL.data() + fet, on - fet);
                    fet = on;
                    if (ambClic) clic.dispara (k % std::max (1, tr.num) == 0);
                }
            }
        }
        else
        {
        const double spb = fs * 60.0 / pla.tempo;
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
        slotNotaTeclat.fill (-1);
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
        // Els instruments del pla que sona, a la freqüència nova (el fil d'àudio no corre ara).
        if (pla != nullptr)
            for (auto& t : pla->pistes)
            {
                if (t.instrument != nullptr) t.instrument->prepara (fs, maxBloc);
                for (auto& e : t.inserts) e->prepara (fs, maxBloc);
            }
        if (pla != nullptr)
            for (auto& e : pla->insertsMaster) e->prepara (fs, maxBloc);
        estat.store ((int) Estat::Aturat, std::memory_order_release);
    }

    bool MotorEstudi::enviaTeclat (std::uint8_t e, std::uint8_t d1, std::uint8_t d2, int desti) noexcept
    {
        // slotNotaTeclat: -1 = no se sap; -2 = l'encesa va anar al Diagnòstic; >= 0 = la pista on va anar.
        const int tipus = e & 0xF0;
        int slot = desti == -2 ? slotTeclat.load (std::memory_order_relaxed) : desti;
        if (tipus == 0x80 || (tipus == 0x90 && d2 == 0))
        {
            // L'apagada va on va anar l'encesa (encara que mentrestant s'hagi canviat de pista).
            const int abans = slotNotaTeclat[(size_t) (d1 & 127)];
            slotNotaTeclat[(size_t) (d1 & 127)] = -1;
            if (abans == -2) return false;
            if (abans >= 0) slot = abans;
        }
        else if (tipus == 0x90)
            slotNotaTeclat[(size_t) (d1 & 127)] = slot >= 0 ? slot : -2;
        if (slot < 0) return false;
        directeTeclat.posa ({ slot, e, d1, d2 });
        return true;
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
            mesclador.canviDePla (pla, p);
            if (pla != nullptr && ! vells.posa (pla)) { /* no hauria de passar mai: es perd (no s'esborra aquí) */ }
            pla = p;
        }

        // Missatges en directe d'aquest bloc (de la pantalla i del teclat).
        numDirecte = 0;
        EventDirecte d;
        while (numDirecte < (int) directeBloc.size() && directeUI.treu (d)) directeBloc[(size_t) numDirecte++] = d;
        const int primerTeclat = numDirecte;
        while (numDirecte < (int) directeBloc.size() && directeTeclat.treu (d)) directeBloc[(size_t) numDirecte++] = d;

        // Gravant: el que arriba del teclat a la pista que grava, amb el temps de la línia de temps.
        const int sg = slotGravaMidi.load (std::memory_order_relaxed);
        if (sg >= 0 && ((Estat) estat.load() == Estat::Gravant || (Estat) estat.load() == Estat::Comptant))
            for (int k = primerTeclat; k < numDirecte; ++k)
                if (directeBloc[(size_t) k].slot == sg)
                    gravats.posa ({ (double) pos / fs, directeBloc[(size_t) k].estat, directeBloc[(size_t) k].dada1, directeBloc[(size_t) k].dada2 });

        Ordre o;
        while (cuaOrdres.treu (o))
        {
            switch (o.tipus)
            {
                case Ordre::Tipus::Reprodueix:
                    if ((Estat) estat.load() == Estat::Comptant || (Estat) estat.load() == Estat::Gravant) break;
                    pos = (long long) std::llround (o.t * fs);
                    entraARuta();
                    compteFins = pos;
                    mesclador.reinicia();
                    estat.store ((int) Estat::Sonant, std::memory_order_release);
                    break;
                case Ordre::Tipus::Atura:
                    estat.store ((int) Estat::Aturat, std::memory_order_release);
                    mesclador.apagaNotes();
                    break;
                case Ordre::Tipus::Situa:
                    if ((Estat) estat.load() == Estat::Sonant)
                    {
                        pos = (long long) std::llround (o.t * fs);
                        entraARuta();
                        compteFins = std::min (compteFins, pos);
                        mesclador.reinicia();
                    }
                    break;
                case Ordre::Tipus::Grava:
                    pos = (long long) std::llround (o.t * fs);
                    compteFins = (long long) std::llround (o.t2 * fs);
                    entradaGrava = std::clamp (o.a, 0, maxEntrades - 1);
                    canalsGrava = o.b == 2 ? 2 : 1;
                    tramRuta = -1;   // gravant, sempre lineal
                    mesclador.setSilenci ({});
                    mesclador.reinicia();
                    estat.store ((int) (pos < compteFins ? Estat::Comptant : Estat::Gravant), std::memory_order_release);
                    break;
            }
        }
        posicio.store ((double) pos / fs, std::memory_order_relaxed);
        tramPublic.store (getEstat() == Estat::Aturat ? -1 : tramRuta, std::memory_order_relaxed);
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
        if (pla == nullptr)
            return;
        if (ara == Estat::Aturat)
        {
            // Aturat: només els instruments (el que es toca en directe i les cues de les notes).
            int fet = 0;
            while (fet < n)
            {
                const int m = std::min (n - fet, maxBloc);
                std::fill (mixL.begin(), mixL.begin() + m, 0.0f);
                std::fill (mixR.begin(), mixR.begin() + m, 0.0f);
                mesclador.processa (*pla, pos, m, mixL.data(), mixR.data(), false, vus.data(), false,
                                    fet == 0 ? directeBloc.data() : nullptr, fet == 0 ? numDirecte : 0);
                sumaSortida (sortides, numSortides, fet, m);
                fet += m;
            }
            return;
        }

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
            // Estructura: el tram acaba al seu final i salta al següent (o s'atura).
            const bool ambRuta = ! gravant && tramRuta >= 0 && tramRuta < (int) pla->ruta.size();
            const long long fiTram = ambRuta ? (long long) std::llround (pla->ruta[(size_t) tramRuta].fi * fs) : 0;
            if (ambRuta && pos + m > fiTram) m = (int) std::max<long long> (0, fiTram - pos);

            std::fill (mixL.begin(), mixL.begin() + m, 0.0f);
            std::fill (mixR.begin(), mixR.begin() + m, 0.0f);
            const bool ambClic = pla->clic || pos < compteFins;
            mesclador.processa (*pla, pos, m, mixL.data(), mixR.data(), ambClic, vus.data(), true,
                                fet == 0 ? directeBloc.data() : nullptr, fet == 0 ? numDirecte : 0);

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

            sumaSortida (sortides, numSortides, fet, m);

            pos += m;
            fet += m;
            if (ambRuta && pos >= fiTram)
            {
                if (++tramRuta >= (int) pla->ruta.size())
                {
                    // Final de l'estructura: s'atura (la resta del bloc, en silenci).
                    tramRuta = -1;
                    mesclador.setSilenci ({});
                    mesclador.apagaNotes();
                    estat.store ((int) Estat::Aturat, std::memory_order_release);
                    break;
                }
                pos = (long long) std::llround (pla->ruta[(size_t) tramRuta].ini * fs);
                compteFins = std::min (compteFins, pos);   // sense compte d'entrada (si no, sonaria la claqueta)
                mesclador.setSilenci (pla->ruta[(size_t) tramRuta].silenci);
                mesclador.reinicia();
                continue;
            }
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

    void MotorEstudi::entraARuta() noexcept
    {
        // Amb estructura: el tram on cau el capçal (el primer que el conté) o, si no n'hi ha cap, el primer tram.
        tramRuta = -1;
        if (pla == nullptr || pla->ruta.empty()) { mesclador.setSilenci ({}); return; }
        const double t = (double) pos / fs;
        for (int i = 0; i < (int) pla->ruta.size(); ++i)
            if (t >= pla->ruta[(size_t) i].ini - 1e-9 && t < pla->ruta[(size_t) i].fi) { tramRuta = i; break; }
        if (tramRuta < 0) { tramRuta = 0; pos = (long long) std::llround (pla->ruta[0].ini * fs); }
        mesclador.setSilenci (pla->ruta[(size_t) tramRuta].silenci);
    }

    void MotorEstudi::sumaSortida (float* const* sortides, int numSortides, int fet, int m) noexcept
    {
        // 0.17: el màster surt pel parell triat; si la targeta no el té, per l'1/2.
        int c0 = pla != nullptr ? 2 * pla->sortidaMaster : 0;
        if (c0 + 1 >= numSortides) c0 = 0;
        if (numSortides >= 2)
        {
            if (sortides[c0] != nullptr) for (int i = 0; i < m; ++i) sortides[c0][fet + i] = std::clamp (sortides[c0][fet + i] + mixL[(size_t) i], -1.0f, 1.0f);
            if (sortides[c0 + 1] != nullptr) for (int i = 0; i < m; ++i) sortides[c0 + 1][fet + i] = std::clamp (sortides[c0 + 1][fet + i] + mixR[(size_t) i], -1.0f, 1.0f);
        }
        else if (numSortides == 1 && sortides[0] != nullptr)
            for (int i = 0; i < m; ++i) sortides[0][fet + i] = std::clamp (sortides[0][fet + i] + 0.5f * (mixL[(size_t) i] + mixR[(size_t) i]), -1.0f, 1.0f);

        vusSortida[0].processa (mixL.data(), m);
        vusSortida[1].processa (mixR.data(), m);
    }

    //==========================================================================
    std::pair<double, double> trosPerExportar (const Projecte& p)
    {
        if (p.bucle.marcat && p.bucle.actiu && p.bucle.a > p.bucle.de)
        {
            return { edicio::iniciBucle (p), edicio::fiBucle (p) };
        }
        const bool hiHaSolo = std::any_of (p.pistes.begin(), p.pistes.end(), [] (const Pista& t) { return t.solo; });
        double a = 1.0e18, b = -1.0e18;
        for (const auto& t : p.pistes)
        {
            if (t.mut || (hiHaSolo && ! t.solo)) continue;
            for (const auto& c : t.clips)
            {
                if (c.midi && ! c.mut && esMidi (t) && ! c.notes.empty())
                {
                    // De la primera nota que sona a l'última (més una mica de cua perquè s'apagui).
                    const double tick0 = midi::ticks (p, c.pos);
                    for (const auto& n : c.notes)
                    {
                        const double ini = midi::segons (p, tick0 + (double) n.ini);
                        if (ini < c.iniciVisible() - 1e-9 || ini >= c.fiVisible()) continue;
                        a = std::min (a, ini);
                        b = std::max (b, std::min (c.fiVisible(), midi::segons (p, tick0 + (double) (n.ini + n.dur))) + 1.0);
                    }
                    continue;
                }
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
