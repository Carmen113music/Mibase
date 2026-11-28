#include "mibase/MotorEstudi.h"

#include <chrono>
#include <semaphore>
#include <thread>
#if defined (__x86_64__) || defined (_M_X64) || defined (__i386__) || defined (_M_IX86)
 #include <immintrin.h>
#endif
#include "mibase/Loudness.h"
#include "mibase/Midi.h"
#include "mibase/Edicio.h"
#include "mibase/Elastic.h"
#include "mibase/Harmonia.h"

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
            if (tocaAcordsSols (t))   // 0.91
                for (const auto& n : notesAcordsSols (p, t)) pp.notes.push_back (n);
            std::stable_sort (pp.notes.begin(), pp.notes.end(), [] (const Pla::Nota& a, const Pla::Nota& b) { return a.ini < b.ini; });
            std::stable_sort (pp.events.begin(), pp.events.end(), [] (const Pla::Event& a, const Pla::Event& b) { return a.t < b.t; });
        }
    }

    ResumAcordsClips resumAcordsClips (const Pista& t)
    {
        ResumAcordsClips r;
        for (const auto& c : t.clips)
        {
            if (! c.midi) continue;
            ++r.clips;
            if (c.mut) ++r.muts;
            else if (! c.blocs.empty()) ++r.patrons;
            else if (! c.notes.empty()) ++r.notes;
            else ++r.acords;
        }
        return r;
    }

    bool tocaAcordsSols (const Pista& t)
    {
        if (! esMidi (t) || ! t.segueixAcords) return false;
        const auto r = resumAcordsClips (t);
        return r.clips == 0 || r.acords > 0;
    }

    std::vector<Pla::Nota> notesAcordsSols (const Projecte& p, const Pista& t)
    {
        std::vector<Pla::Nota> r;
        if (! tocaAcordsSols (t)) return r;
        // 0.94: on poden sonar: tota la cançó (sense clips) o els trossos destapats dels clips buits
        std::vector<std::pair<double, double>> zones;
        {
            std::vector<const Clip*> clips;
            bool cap = true;
            for (const auto& c : t.clips)
                if (c.midi) { cap = false; if (! c.mut) clips.push_back (&c); }
            if (cap) zones.push_back ({ -1.0e9, 1.0e9 });
            for (size_t k = 0; k < clips.size(); ++k)
            {
                const auto& c = *clips[k];
                if (! c.notes.empty() || ! c.blocs.empty()) continue;
                std::vector<std::pair<double, double>> sobre;
                for (size_t j = k + 1; j < clips.size(); ++j) sobre.push_back ({ clips[j]->iniciVisible(), clips[j]->fiVisible() });
                for (auto& z : destapat (c.iniciVisible(), c.fiVisible(), sobre)) zones.push_back (z);
            }
        }
        struct T { double a, b; std::string acord; };
        std::vector<T> trams;
        for (const auto& ta : p.pistes)
        {
            if (! esAcords (ta)) continue;
            for (const auto& k : ta.clips)
                if (k.esAcord() && ! k.mut && k.fiVisible() - k.iniciVisible() > 0.01) trams.push_back ({ k.iniciVisible(), k.fiVisible(), k.acord });
        }
        std::stable_sort (trams.begin(), trams.end(), [] (const T& x, const T& y) { return x.a < y.a; });
        for (size_t i = 0; i + 1 < trams.size(); ++i) trams[i].b = std::min (trams[i].b, trams[i + 1].a);   // el següent talla l'anterior
        for (const auto& x0 : trams)
          for (const auto& z : zones)
        {
            T x = x0;
            x.a = std::max (x.a, z.first);
            x.b = std::min (x.b, z.second);
            if (x.b - x.a < 0.005) continue;
            auto notes = harmonia::notesPad (harmonia::llegeixAcord (x.acord), 4, harmonia::Voicing::Tancat);
            if (notes.empty()) continue;
            // al voltant del do central, com la mà dreta d'un pianista
            int mx = 0; for (int n : notes) mx = std::max (mx, n);
            if (mx > 76) for (auto& n : notes) n -= 12;
            for (int n : notes)
            {
                Pla::Nota pn;
                pn.ini = x.a;
                pn.fi = x.b - 0.002;   // un bri abans, perquè l'acord següent torni a atacar
                pn.to = (std::uint8_t) std::clamp (n, 0, 127);
                pn.vel = 90;
                pn.canal = 0;
                r.push_back (pn);
            }
        }
        return r;
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
        std::vector<const Insert*> viusMaster;   // 0.56
        if (efectes)
            for (const auto& x : p.insertsMaster)
                if (x.actiu)
                    if (auto e = efectes (x)) { pla->insertsMaster.push_back (std::move (e)); pla->encesAmbRMMaster.push_back (x.encesAmbRM ? 1 : 0); viusMaster.push_back (&x); }
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

        const auto solo = edicio::estatSolo (p);
        std::vector<std::vector<const Insert*>> vius;   // 0.56: quin insert del projecte és cada efecte del pla
        for (const auto& t : p.pistes)
        {
            vius.emplace_back();
            Pla::Pista pp;
            pp.slot = slotDePista (t);
            pp.canals = t.canals == 2 ? 2 : 1;
            pp.armada = t.grava;   // 0.68
            if (t.grava && t.escolta && t.tipus == "audio") pp.escoltaEntrada = std::max (0, t.entrada);   // 0.71
            pp.guany = guanyDb (t.faderDb);
            pp.pan = (float) std::clamp (t.pan, -1.0, 1.0);
            const int estatSolo = solo[pla->pistes.size()];
            pp.sona = estatSolo > 0;
            pp.obert = estatSolo == 2;
            const bool ambVeu = corregides != nullptr && ! esMidi (t) && veuActiva (t);
            for (const auto& c : t.clips)
            {
                if (c.mut || c.presa.empty()) continue;
                const std::shared_ptr<const AudioPresa>* au = nullptr;
                if (auto it0 = audios.find (c.presa); it0 != audios.end()) au = &it0->second;
                if (ambVeu)
                    if (auto ic = corregides->find (c.presa); ic != corregides->end() && ic->second != nullptr && ic->second->longitud > 0) au = &ic->second;
                // 0.86: Elastic Audio: el so estirat (si ja està calculat; si no, l'original mentre s'acaba).
                if (elastic::actiu (c) && corregides != nullptr && au != nullptr)
                {
                    const bool deVeu = ambVeu && corregides->count (c.presa) > 0;
                    if (auto ie = corregides->find (elastic::clau (c, deVeu)); ie != corregides->end() && ie->second != nullptr && ie->second->longitud > 0)
                        au = &ie->second;
                }
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
                    const auto& u = pp.clips[j];
                    const double a = u.pos + u.inici + u.fadeIn,
                                 b = u.pos + u.fi - u.fadeOut;
                    // 0.31.1: i hi fa el fade contrari (si els fades del de dalt toquen aquest clip)
                    const double c0 = c.pos + c.inici, c1 = c.pos + c.fi, u0 = u.pos + u.inici, u1 = u.pos + u.fi;
                    const bool tocaIn = u.fadeIn > 0.0 && u0 < c1 && a > c0, tocaOut = u.fadeOut > 0.0 && b < c1 && u1 > c0;
                    // 0.32: si el de sota ja fa el seu fade en aquell tros (el crossfade fet amb la selecció i la X posa el
                    // fade out al de sota i el fade in al de dalt), no se n'hi afegeix cap altre: baixaria dues vegades.
                    const double eps = 1.0e-6;
                    const bool jaSurt = c.fadeOut > 0.0 && c1 <= u0 + u.fadeIn + eps;    // el de sota s'acaba fent fade out dins el fade in del de dalt
                    const bool jaEntra = c.fadeIn > 0.0 && c0 >= u1 - u.fadeOut - eps;   // el de sota comença fent fade in dins el fade out del de dalt
                    Pla::Clip::Tapa x { u0, u0 + u.fadeIn, u1 - u.fadeOut, u1 };
                    bool posaIn = tocaIn && ! jaSurt, posaOut = tocaOut && ! jaEntra;
                    if (! posaIn) { x.a0 = x.b0; x.a1 = x.b0; }
                    if (! posaOut) { x.b0 = x.a1; x.b1 = x.a1; if (! posaIn) x.b1 = x.a0; }
                    if ((posaIn || posaOut) && u0 < c1 && u1 > c0)
                        c.tapes.push_back (x);
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
                        if (auto e = efectes (x)) { pp.inserts.push_back (std::move (e)); pp.encesAmbRM.push_back (x.encesAmbRM ? 1 : 0); vius.back().push_back (&x); }
            // 0.56: automatització
            for (const auto& a : t.autos)
            {
                if (! a.activa || a.punts.empty()) continue;
                std::string ii, pid;
                const int tipus = automatitzacio::llegeixObjectiu (a.objectiu, ii, pid);
                if (tipus == 0) { pp.autoVolum = automatitzacio::corbaEnSegons (p, a); pp.lliureVolum = automatitzacio::bandera (a.id); }
                else if (tipus == 1) { pp.autoPan = automatitzacio::corbaEnSegons (p, a); pp.lliurePan = automatitzacio::bandera (a.id); }
                else if (tipus == 2)
                {
                    for (size_t k = 0; k < vius.back().size(); ++k)
                        if (vius.back()[k]->id == ii)
                        {
                            Parametritzable* obj = pp.inserts[k].get();
                            const int ix = obj->indexParametre (pid);
                            if (ix >= 0)
                            {
                                pp.autoParams.push_back ({ obj, ix, false, automatitzacio::corbaEnSegons (p, a), -1.0f, automatitzacio::bandera (a.id) });
                                pp.autoInserts = true;
                            }
                        }
                }
                else if (tipus == 3 && pp.instrument != nullptr)
                {
                    Parametritzable* obj = pp.instrument.get();
                    const int ix = obj->indexParametre (pid);
                    if (ix >= 0) pp.autoParams.push_back ({ obj, ix, true, automatitzacio::corbaEnSegons (p, a), -1.0f, automatitzacio::bandera (a.id) });
                }
            }
            pp.scInserts.assign (pp.inserts.size(), -1);
            pla->pistes.push_back (std::move (pp));
        }

        // 0.33: busos, sortides i enviaments.
        const int n = (int) p.pistes.size();
        for (int i = 0; i < n; ++i)
            if (esBus (p.pistes[(size_t) i]) && pla->numBusos < maxBusos)
            {
                pla->pistes[(size_t) i].bus = pla->numBusos++;
                pla->pistes[(size_t) i].canals = 2;
            }
        auto busDe = [&] (const std::string& id)   // índex de la pista del bus (o -1)
        {
            const int j = edicio::pistaPerId (p, id);
            return j >= 0 && pla->pistes[(size_t) j].bus >= 0 ? j : -1;
        };
        std::vector<std::vector<int>> cap ((size_t) n);   // a quines pistes va cada una (sortida i enviaments)
        std::vector<int> entren ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            const auto& t = p.pistes[(size_t) i];
            if (const int j = busDe (t.sortida); j >= 0 && j != i) cap[(size_t) i].push_back (j);
            for (const auto& e : t.enviaments)
                if (e.actiu && ! e.mut)
                    if (const int j = busDe (e.desti); j >= 0 && j != i) cap[(size_t) i].push_back (j);
            for (int j : cap[(size_t) i]) ++entren[(size_t) j];
        }
        // 0.56: sidechain. Cada efecte amb fonts té un buffer; la font s'ha de calcular abans que la pista de l'efecte.
        struct Sc { int font, desti, buf; float guany; int punt; };
        std::vector<Sc> scs;
        auto fontsDe = [&] (const Insert& x, const std::shared_ptr<Efecte>& e, int desti) -> int
        {
            if (x.sidechain.empty() || e == nullptr || ! e->admetSidechain() || pla->numSidechain >= maxSidechain) return -1;
            int buf = -1;
            for (const auto& f : x.sidechain)
            {
                if (! f.actiu) continue;
                const int j = edicio::pistaPerId (p, f.pista);
                if (j < 0 || j == desti || esAcords (p.pistes[(size_t) j])) continue;
                if (buf < 0) buf = pla->numSidechain++;
                scs.push_back ({ j, desti, buf, guanyDb (f.nivellDb), std::clamp (f.punt, 0, 2) });
            }
            return buf;
        };
        for (int i = 0; i < n; ++i)
            for (size_t k = 0; k < vius[(size_t) i].size(); ++k)
                pla->pistes[(size_t) i].scInserts[k] = fontsDe (*vius[(size_t) i][k], pla->pistes[(size_t) i].inserts[k], i);
        for (size_t k = 0; k < viusMaster.size(); ++k)
            pla->scMaster.push_back (fontsDe (*viusMaster[k], pla->insertsMaster[k], -1));
        for (const auto& x : scs)
            if (x.desti >= 0)
            {
                cap[(size_t) x.font].push_back (x.desti);
                ++entren[(size_t) x.desti];
            }
        // Ordre de càlcul: cada pista abans dels busos on va.
        std::vector<char> posada ((size_t) n, 0);
        for (bool canvi = true; canvi;)
        {
            canvi = false;
            for (int i = 0; i < n; ++i)
                if (! posada[(size_t) i] && entren[(size_t) i] == 0)
                {
                    posada[(size_t) i] = 1;
                    pla->ordre.push_back (i);
                    for (int j : cap[(size_t) i]) --entren[(size_t) j];
                    canvi = true;
                }
        }
        for (int i = 0; i < n; ++i)
        {
            auto& pp = pla->pistes[(size_t) i];
            if (! posada[(size_t) i])
            {
                // Un cercle (la pantalla no el deixa fer; un fitxer tocat a mà, sí): aquesta va directa a la sortida.
                pla->ordre.push_back (i);
                continue;
            }
            const auto& t = p.pistes[(size_t) i];
            if (const int j = busDe (t.sortida); j >= 0 && j != i && posada[(size_t) j]) pp.desti = pla->pistes[(size_t) j].bus;
            else if (t.sortidaTargeta >= 0 && t.sortidaTargeta < maxSortidesDirectes)   // 1.17: directa a la targeta
            {
                pp.sortidaDirecta = t.sortidaTargeta;
                pp.canalsDirecta = t.canalsTargeta == 1 ? 1 : 2;
            }
            for (const auto& e : t.enviaments)
            {
                if (! e.actiu || e.mut) continue;
                const int j = busDe (e.desti);
                if (j < 0 || j == i || ! posada[(size_t) j]) continue;
                Pla::Pista::Envia x;
                x.desti = pla->pistes[(size_t) j].bus;
                const float g = guanyDb (e.nivellDb), pan = (float) std::clamp (e.pan, -1.0, 1.0);
                x.guanyL = g * (pan <= 0.0f ? 1.0f : 1.0f - pan);
                x.guanyR = g * (pan >= 0.0f ? 1.0f : 1.0f + pan);
                x.mode = std::clamp (e.mode, 0, 2);
                pp.envia.push_back (x);
            }
        }
        // 0.56: els enviaments de sidechain que han quedat ben ordenats (si hi hagués un cercle, no sonen).
        std::vector<int> posOrdre ((size_t) n, -1);
        for (size_t q = 0; q < pla->ordre.size(); ++q) posOrdre[(size_t) pla->ordre[q]] = (int) q;
        for (const auto& x : scs)
        {
            if (x.desti >= 0 && (! posada[(size_t) x.font] || ! posada[(size_t) x.desti] || posOrdre[(size_t) x.font] >= posOrdre[(size_t) x.desti]))
                continue;
            auto& pf = pla->pistes[(size_t) x.font];
            pf.scEnvia.push_back ({ x.buf, x.guany, x.punt });
            if (x.punt < 2) pf.scSempre = true;
        }
        return pla;
    }

    //==========================================================================
    void Mesclador::prepara (double nouFs, int maxBloc)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        pL.assign ((size_t) std::max (16, maxBloc), 0.0f);
        pR.assign ((size_t) std::max (16, maxBloc), 0.0f);
        maxN = (int) pL.size();
        busL.assign ((size_t) maxBusos * (size_t) maxN, 0.0f);
        busR.assign ((size_t) maxBusos * (size_t) maxN, 0.0f);
        dirBuf.assign ((size_t) maxSortidesDirectes * (size_t) maxN, 0.0f);   // 1.17
        usatsDir = 0;
        scL.assign ((size_t) maxSidechain * (size_t) maxN, 0.0f);   // 0.56
        scR.assign ((size_t) maxSidechain * (size_t) maxN, 0.0f);
        gEnvia.assign ((size_t) maxSlots * maxEnviaSuau * 2, -1.0f);
        clic.prepara (fs);
        if (slots.size() != (size_t) maxSlots) slots.assign ((size_t) maxSlots, EstatSlot {});
        fil0.evs.assign ((size_t) maxEventsBloc, EventInstrument {});
        feines.assign ((size_t) maxFeines, Feina {});                        // 1.9
        llistaApart.assign ((size_t) maxFeines, 0);                          // 1.10
        apartL.assign ((size_t) maxSlots * (size_t) maxN, 0.0f);
        apartR.assign ((size_t) maxSlots * (size_t) maxN, 0.0f);
        reinicia();
    }

    namespace
    {
        float picDe (const float* L, const float* R, int n) noexcept
        {
            float p = 0.0f;
            for (int i = 0; i < n; ++i) p = std::max (p, std::max (std::abs (L[i]), std::abs (R[i])));
            return p;
        }
        void puja (std::atomic<float>& a, float p, int n, double fs) noexcept
        {
            const float ara = a.load (std::memory_order_relaxed) * (float) std::exp (-(double) n / (0.5 * std::max (1.0, fs)));
            a.store (std::max (ara, p), std::memory_order_relaxed);
        }
        /** Si un tros té valors no vàlids (NaN, infinit o més de +60 dB), el deixa en silenci i ho diu. */
        bool valid (float* L, float* R, int n) noexcept
        {
            float s = 0.0f;
            for (int i = 0; i < n; ++i) s += std::abs (L[i]) + std::abs (R[i]);
            if (std::isfinite (s) && s < 2000.0f * (float) n) return true;
            std::fill (L, L + n, 0.0f);
            std::fill (R, R + n, 0.0f);
            return false;
        }
    }

    void Mesclador::reinicia() noexcept
    {
        gFet.fill (false);
        std::fill (gEnvia.begin(), gEnvia.end(), -1.0f);
        masterFet = false;
        clic.reinicia();
        apagaNotes();
        for (auto& s : slots) { s.tornat = false; s.senseMarge = false; }   // 0.98
        margeInici = true;
    }

    void Mesclador::reiniciaBucle() noexcept
    {
        gFet.fill (false);
        std::fill (gEnvia.begin(), gEnvia.end(), -1.0f);
        masterFet = false;
        clic.reinicia();
        for (auto& s : slots)
        {
            if (s.inst == nullptr) continue;
            if (s.tornat) { s.tornat = false; s.senseMarge = true; }   // 0.98: ja ha tornat abans (PDC): res a apagar
            else s.apagarTou = true;
        }
        margeInici = true;
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

    void Mesclador::afegeixEvent (Fil& f, int offset, std::uint8_t estat, std::uint8_t d1, std::uint8_t d2) noexcept
    {
        auto& evs = f.evs;
        if (f.numEvs >= (int) evs.size()) return;
        // Ordenat per offset (inserció: n'hi ha pocs); al mateix offset, en l'ordre d'arribada.
        int i = f.numEvs++;
        while (i > 0 && evs[(size_t) (i - 1)].offset > offset) { evs[(size_t) i] = evs[(size_t) (i - 1)]; --i; }
        evs[(size_t) i] = { offset, estat, d1, d2 };
    }

    void Mesclador::instrumentPista (Fil& f, const Pla::Pista& t, long long pos, int n, bool reproduint, const EventDirecte* directe, int numDirecte) noexcept
    {
        auto& es = slots[(size_t) (t.slot % maxSlots)];
        auto* inst = t.instrument.get();
        f.numEvs = 0;

        if (es.inst != inst)
        {
            es.inst = inst;
            es.n = 0;
            es.apagar = false;
            es.apagarTou = false;
        }
        if (es.apagar)
        {
            es.apagar = false;
            es.apagarTou = false;
            es.n = 0;
            inst->apagaTot();
        }
        if (es.apagarTou)   // 0.71: nota per nota, abans de les notes noves del mateix instant
        {
            es.apagarTou = false;
            apagaTou (f, es, 0);
        }
        // En directe (piano roll, teclat): al començament del bloc.
        for (int k = 0; k < numDirecte; ++k)
            if (directe[k].slot == t.slot)
                afegeixEvent (f, 0, directe[k].estat, directe[k].dada1, directe[k].dada2);

        // després d'un salt, les notes que cauen una mica abans també sonen: 1 ms (arrodoniments) més el que la pista
        // llegeix avançada (PDC): si no, en fer play just sobre una nota amb un plugin que retarda, no sonava
        const long long ms1 = std::max (1LL, std::llround (fs * 0.001));
        const long long marge = margeAra && ! es.senseMarge ? ms1 + std::max (0LL, pos - posReal) : 0;
        es.senseMarge = false;
        // 0.98: la pista llegeix avançada (PDC) i el bucle torna: el tros que passa del final es llegeix de l'inici.
        const long long llargB = bucleFiS - bucleIniS;
        if (llargB <= 0 || ! reproduint) es.tornat = false;   // sense bucle (o aturat): res a esperar
        if (llargB > 0 && reproduint && pos > posReal && posReal < bucleFiS)
        {
            if (pos >= bucleFiS)
            {
                if (! es.tornat) { apagaTou (f, es, 0); es.tornat = true; }
                llegeixMidi (f, t, es, bucleIniS + (pos - bucleFiS) % llargB, n, 0, reproduint, 0);
                return;
            }
            if (pos + n > bucleFiS)
            {
                const int talla = (int) (bucleFiS - pos);
                llegeixMidi (f, t, es, pos, talla, 0, reproduint, marge);
                apagaTou (f, es, talla);
                es.tornat = true;
                llegeixMidi (f, t, es, bucleIniS, n - talla, talla, reproduint, ms1);
                return;
            }
        }
        llegeixMidi (f, t, es, pos, n, 0, reproduint, marge);
    }

    void Mesclador::apagaTou (Fil& f, EstatSlot& es, int off) noexcept
    {
        std::uint16_t canals = 0;
        for (int k = 0; k < es.n; ++k)
        {
            afegeixEvent (f, off, (std::uint8_t) (0x80 | es.act[(size_t) k].canal), es.act[(size_t) k].to, 0);
            canals = (std::uint16_t) (canals | (1u << (es.act[(size_t) k].canal & 15)));
        }
        for (int c = 0; c < 16; ++c)   // el pedal, amunt (si no, les notes es quedarien sostingudes)
            if (canals & (1u << c)) afegeixEvent (f, off, (std::uint8_t) (0xB0 | c), 64, 0);
        es.n = 0;
    }

    void Mesclador::llegeixMidi (Fil& f, const Pla::Pista& t, EstatSlot& es, long long pos, int n, int base, bool reproduint, long long marge) noexcept
    {
        if (n <= 0) return;
        // 0.71: després d'un salt (play, bucle, tram), les notes que cauen fins a 1 ms abans del punt també sonen
        // (per arrodoniments, una nota just a l'inici del bucle podia quedar una mostra abans i no sonar mai).
        const long long desde = pos - marge;

        // Notes que s'acaben dins aquest bloc.
        for (int k = 0; k < es.n;)
        {
            auto& a = es.act[(size_t) k];
            if (a.fi < pos + n)
            {
                afegeixEvent (f, base + (int) std::clamp (a.fi - pos, 0LL, (long long) n - 1), (std::uint8_t) (0x80 | a.canal), a.to, 0);
                es.act[(size_t) k] = es.act[(size_t) (es.n - 1)];
                --es.n;
            }
            else ++k;
        }

        if (reproduint)
        {
            // Controladors i altres missatges.
            {
                auto it = std::lower_bound (t.events.begin(), t.events.end(), desde, [this] (const Pla::Event& e, long long p)
                                            { return std::llround (e.t * fs) < p; });
                for (; it != t.events.end(); ++it)
                {
                    const long long s = std::llround (it->t * fs);
                    if (s >= pos + n) break;
                    afegeixEvent (f, base + (int) std::max (0LL, s - pos), it->estat, it->dada1, it->dada2);
                }
            }
            // Notes que comencen dins aquest bloc (si la pista no està silenciada).
            if (t.sona && ! silenciAra[(size_t) (t.slot % maxSlots)] && ! nomesCua)
            {
                auto it = std::lower_bound (t.notes.begin(), t.notes.end(), desde, [this] (const Pla::Nota& x, long long p)
                                            { return std::llround (x.ini * fs) < p; });
                for (; it != t.notes.end(); ++it)
                {
                    const long long s = std::max (pos, std::llround (it->ini * fs));
                    if (s >= pos + n) break;
                    const int off = base + (int) (s - pos);
                    const long long fi = std::max (s + 1, std::llround (it->fi * fs));
                    // La mateixa nota ja sona: s'apaga abans de tornar-la a tocar.
                    bool ja = false;
                    for (int k = 0; k < es.n; ++k)
                    {
                        auto& a = es.act[(size_t) k];
                        if (a.to == it->to && a.canal == it->canal)
                        {
                            afegeixEvent (f, off, (std::uint8_t) (0x80 | a.canal), a.to, 0);
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
                    afegeixEvent (f, off, (std::uint8_t) (0x90 | it->canal), it->to, it->vel);
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
                        afegeixEvent (f, base + (int) (fi - pos), (std::uint8_t) (0x80 | it->canal), it->to, 0);
                    }
                }
            }
        }
    }

    void Mesclador::sonaClip (Fil& f, const Pla::Clip& c, int canalsPista, long long pos, int n) noexcept
    {
        for (const auto& t : c.trams)
            sonaTram (f, c, t.first, t.second, canalsPista, pos, n);
    }

    void Mesclador::sonaTram (Fil& f, const Pla::Clip& c, double de, double fins, int canalsPista, long long pos, int n) noexcept
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
            // 0.31.1: crossfade: el contrari del fade del clip de dalt que el tapa
            if (! c.tapes.empty())
            {
                const double t = (double) (pos + i) / fs;
                for (const auto& x : c.tapes)
                {
                    if (t < x.a0 || t >= x.b1) continue;
                    double g = 1.0;
                    if (t < x.a1) g = std::min (g, (t - x.a0) / (x.a1 - x.a0));
                    if (t > x.b0) g = std::min (g, (x.b1 - t) / (x.b1 - x.b0));
                    env *= 1.0 - std::clamp (g, 0.0, 1.0);
                }
            }

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
                f.oL[f.escriuDes + i] += (float) env * l;
                f.oR[f.escriuDes + i] += (float) env * r;
            }
            else
                f.oL[f.escriuDes + i] += (float) env * (a.canals > 1 ? 0.5f * (l + r) : l);
        }
    }

    void Mesclador::envia (const Pla::Pista& t, int mode, const float* xL, const float* xR, int n, int nb) noexcept
    {
        for (size_t k = 0; k < t.envia.size(); ++k)
        {
            const auto& e = t.envia[k];
            if (e.mode != mode || e.desti < 0 || e.desti >= nb) continue;
            float l0 = e.guanyL, r0 = e.guanyR;
            if (k < (size_t) maxEnviaSuau)
            {
                float* g = gEnvia.data() + ((size_t) (t.slot % maxSlots) * maxEnviaSuau + k) * 2;
                if (g[0] >= 0.0f) { l0 = g[0]; r0 = g[1]; }
                g[0] = e.guanyL;
                g[1] = e.guanyR;
            }
            float* const dL = busL.data() + (std::ptrdiff_t) e.desti * maxN;
            float* const dR = busR.data() + (std::ptrdiff_t) e.desti * maxN;
            const float dl = (e.guanyL - l0) / (float) n, dr = (e.guanyR - r0) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                dL[i] += xL[i] * (l0 + dl * (float) i);
                dR[i] += xR[i] * (r0 + dr * (float) i);
            }
        }
    }

    void Mesclador::enviaSc (const Pla::Pista& t, int punt, const float* xL, const float* xR, int n, int ns) noexcept
    {
        for (const auto& e : t.scEnvia)
        {
            if (e.punt != punt || e.buf < 0 || e.buf >= ns) continue;
            float* const dL = scL.data() + (std::ptrdiff_t) e.buf * maxN;
            float* const dR = scR.data() + (std::ptrdiff_t) e.buf * maxN;
            for (int i = 0; i < n; ++i)
            {
                dL[i] += xL[i] * e.guany;
                dR[i] += xR[i] * e.guany;
            }
        }
    }

    void Mesclador::efectes (const Pla::Pista& t, const std::vector<std::shared_ptr<Efecte>>& v, const std::vector<int>& sc, float* xL, float* xR,
                             long long pos, int n, int ns, bool salta, const std::vector<char>* fixos, int llindar) noexcept
    {
        // 0.56: amb paràmetres d'insert automatitzats, a trossos de 128 mostres (el valor canvia a cada tros).
        const int tros = t.autoInserts ? 128 : n;
        for (int ofs = 0; ofs < n; ofs += tros)
        {
            const int m = std::min (tros, n - ofs);
            if (t.autoInserts)
            {
                const double seg = (double) (pos + ofs + m) / fs;
                for (const auto& a : t.autoParams)
                {
                    if (a.instrument || (a.lliure != nullptr && a.lliure->load (std::memory_order_relaxed))) { a.ultim = -1.0f; continue; }
                    const float x = a.corba.a (seg);
                    if (std::abs (x - a.ultim) > 1.0e-6f) { a.obj->posaParametre (a.index, x); a.ultim = x; }
                }
            }
            for (size_t k = 0; k < v.size(); ++k)
            {
                const int b = k < sc.size() ? sc[k] : -1;
                if (b >= 0 && b < ns)
                    v[k]->posaSidechain (scL.data() + (std::ptrdiff_t) b * maxN + ofs, scR.data() + (std::ptrdiff_t) b * maxN + ofs);
                else
                    v[k]->posaSidechain (nullptr, nullptr);
                if (salta && v[k]->latencia() > llindar && ! (fixos != nullptr && k < fixos->size() && (*fixos)[k] != 0))
                    continue;   // 0.68: «Retard mínim» (0.69: amb mínim i els «sempre encesos»)
                v[k]->processa (xL + ofs, xR + ofs, m);
            }
        }
    }

    int Mesclador::retardDe (const std::vector<std::shared_ptr<Efecte>>& v, bool salta, int* saltats, const std::vector<char>* fixos, int llindar) noexcept
    {
        int r = 0;
        for (size_t k = 0; k < v.size(); ++k)
        {
            const auto& e = v[k];
            const int l = e != nullptr ? std::clamp (e->latencia(), 0, 1 << 20) : 0;
            if (l <= 0) continue;
            const bool fix = fixos != nullptr && k < fixos->size() && (*fixos)[k] != 0;
            if (salta && ! fix && l > llindar) { if (saltats != nullptr) ++*saltats; continue; }
            r += l;
        }
        return r;
    }

    //==========================================================================
    // 1.10: l'equip de fils de treball. Cada bloc, el fil d'àudio publica la llista de pistes «a part» i desperta tants
    // ajudants com calguin; tots (també el fil d'àudio) van agafant pistes amb un comptador atòmic fins que no en queda
    // cap, i el fil d'àudio espera (sense dormir: uns pocs microsegons) que s'acabin les que fan els altres. Res de
    // memòria nova ni de bloquejos: només números atòmics i un semàfor per despertar (que no fa esperar qui el dona).
    namespace
    {
        inline void pausaCurta() noexcept
        {
           #if defined (__x86_64__) || defined (_M_X64) || defined (__i386__) || defined (_M_IX86)
            _mm_pause();
           #else
            std::this_thread::yield();
           #endif
        }
    }

    class Mesclador::Equip
    {
    public:
        Equip (Mesclador& m, int numAjudants) : mes (m), fils ((size_t) numAjudants)
        {
            for (auto& f : fils) f.evs.assign ((size_t) maxEventsBloc, EventInstrument {});
            for (int w = 0; w < numAjudants; ++w)
                treballadors.emplace_back ([this, w] { treballa (w); });
        }
        ~Equip()
        {
            atura.store (true, std::memory_order_release);
            sem.release ((std::ptrdiff_t) treballadors.size());
            for (auto& t : treballadors) t.join();
        }
        int mida() const noexcept { return (int) treballadors.size(); }
        std::atomic<long long> treballNs { 0 };   // 1.11: la suma del temps de totes les pistes (de tots els fils)

        /** Fil d'àudio: calcula les pistes de la llista amb l'ajuda de «ajudants» fils. Torna quan totes estan fetes. */
        void fes (const int* llista, int total, int ajudants) noexcept
        {
            llistaAra = llista;
            fets.store (0, std::memory_order_relaxed);
            gen = (gen + 1) & 0xFFFF;
            cursor.store (((std::uint64_t) gen << 48) | ((std::uint64_t) total << 32), std::memory_order_release);
            if (ajudants > 0) sem.release (ajudants);
            while (agafa (mes.fil0)) {}
            while (fets.load (std::memory_order_acquire) < total) pausaCurta();
        }

    private:
        bool agafa (Fil& f) noexcept
        {
            const std::uint64_t v = cursor.fetch_add (1, std::memory_order_acq_rel);
            const int total = (int) ((v >> 32) & 0xFFFF), idx = (int) (v & 0xFFFFFFFFull);
            if (idx >= total) return false;
            const auto& b = mes.bloc;
            const auto t0 = std::chrono::steady_clock::now();
            mes.calculaApart (f, *b.pla, llistaAra[idx], b.n, b.ns, b.reproduint, b.minim, b.llindar, b.directe, b.numDirecte);
            treballNs.fetch_add ((long long) std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::steady_clock::now() - t0).count(),
                                 std::memory_order_relaxed);
            fets.fetch_add (1, std::memory_order_acq_rel);
            return true;
        }
        void treballa (int w)
        {
            if (preparaFilTreballador != nullptr) preparaFilTreballador();
            for (;;)
            {
                sem.acquire();
                if (atura.load (std::memory_order_acquire)) return;
                while (agafa (fils[(size_t) w])) {}
            }
        }

        Mesclador& mes;
        std::vector<Fil> fils;
        std::vector<std::thread> treballadors;
        std::counting_semaphore<> sem { 0 };
        std::atomic<bool> atura { false };
        std::atomic<std::uint64_t> cursor { 0 };   // generació (16 bits) · total (16 bits) · següent (32 bits)
        std::atomic<int> fets { 0 };
        const int* llistaAra = nullptr;
        std::uint64_t gen = 0;
    };

    Mesclador::Mesclador() = default;
    Mesclador::~Mesclador() = default;

    int Mesclador::maxAjudants() noexcept
    {
        const int hw = (int) std::thread::hardware_concurrency();
        return std::clamp (hw - 1, 0, 15);
    }

    void Mesclador::setNuclis (int n)
    {
        n = std::clamp (n, 1, maxAjudants() + 1);
        if (n > 1 && equipViu == nullptr)
        {
            equipViu = std::make_unique<Equip> (*this, maxAjudants());
            equip.store (equipViu.get(), std::memory_order_release);
        }
        nuclis.store (n, std::memory_order_release);
    }

    Mesclador::Guanys Mesclador::guanysDe (const Pla::Pista& t, long long posT, int n) const noexcept
    {
        Guanys g {};
        const int s = t.slot % maxSlots;
        g.porta = t.sona && ! silenciAra[(size_t) s] ? 1.0f : 0.0f;
        // 0.56: el volum i el panorama automatitzats manen sobre el fader i el pom
        g.segAuto = (double) (posT + n) / fs;
        g.llegeixVol = ! t.autoVolum.buida() && ! (t.lliureVolum != nullptr && t.lliureVolum->load (std::memory_order_relaxed));
        g.llegeixPan = ! t.autoPan.buida() && ! (t.lliurePan != nullptr && t.lliurePan->load (std::memory_order_relaxed));
        g.fader = ! g.llegeixVol ? t.guany : (float) automatitzacio::guanyDeVolum (t.autoVolum.a (g.segAuto));
        const float panAra = ! g.llegeixPan ? t.pan : (float) automatitzacio::panDe (t.autoPan.a (g.segAuto));
        g.panL = panAra <= 0.0f ? 1.0f : 1.0f - panAra;
        g.panR = panAra >= 0.0f ? 1.0f : 1.0f + panAra;
        g.obert = t.obert ? 1.0f : 0.0f;
        return g;
    }

    void Mesclador::calculaApart (Fil& fil, const Pla& pla, int q, int n, int ns, bool reproduint, bool minim, int llindar,
                                  const EventDirecte* directe, int numDirecte) noexcept
    {
        // 1.9: el mateix que fa el bucle de processa() per a una pista (fins als efectes), però a la memòria de la pista i
        // sense tocar res compartit: els volums suavitzats només es llegeixen (el bucle els posa al dia després).
        const bool ambOrdre = pla.ordre.size() == pla.pistes.size();
        const auto& t = pla.pistes[ambOrdre ? (size_t) pla.ordre[(size_t) q] : (size_t) q];
        auto& f = feines[(size_t) q];
        const int s = t.slot % maxSlots;
        const long long posT = f.posT, pos = posReal;
        float* const xL = apartL.data() + (std::ptrdiff_t) q * maxN;
        float* const xR = apartR.data() + (std::ptrdiff_t) q * maxN;
        bool instrumentFet = false;
        if (t.instrument != nullptr)
        {
            for (const auto& a : t.autoParams)
                if (a.instrument && ! (a.lliure != nullptr && a.lliure->load (std::memory_order_relaxed)))
                {
                    const float x = a.corba.a ((double) (posT + n / 2) / fs);
                    if (std::abs (x - a.ultim) > 1.0e-6f) { a.obj->posaParametre (a.index, x); a.ultim = x; }
                }
            instrumentPista (fil, t, posT, n, reproduint, directe, numDirecte);
            f.numEvs = fil.numEvs;
            if (fil.numEvs == 0 && ! t.instrument->fentSo()) { f.estat = Feina::SaltaAbans; return; }
            t.instrument->processa (fil.evs.data(), fil.numEvs, xL, xR, n);
            instrumentFet = true;
            f.picInst = picDe (xL, xR, n);
        }
        else if (! reproduint && t.escoltaEntrada < 0)
        {
            f.estat = Feina::SaltaAbans;
            return;
        }
        f.instrumentFet = instrumentFet;

        // Si el bucle no la deixarà sonar, no cal calcular-la (les mateixes condicions, amb els mateixos números).
        const Guanys gu = guanysDe (t, posT, n);
        const bool fet = gFet[(size_t) s];
        const float porta0 = fet ? gPorta[(size_t) s] : gu.porta, fader0 = fet ? gFader[(size_t) s] : gu.fader;
        bool hiHaPre = t.scSempre;
        for (const auto& e : t.envia) hiHaPre = hiHaPre || e.mode == 0;
        if ((porta0 < 1.0e-6f && gu.porta < 1.0e-6f && ! t.scSempre) || (! hiHaPre && fader0 < 1.0e-6f && gu.fader < 1.0e-6f))
        {
            f.estat = Feina::SaltaDespres;
            return;
        }

        if (! instrumentFet)
        {
            std::fill (xL, xL + n, 0.0f);
            if (t.canals == 2) std::fill (xR, xR + n, 0.0f);
            fil.oL = xL; fil.oR = xR;
            if (reproduint && ! nomesCua)
            {
                const long long llargB = bucleFiS - bucleIniS;
                const bool torna = llargB > 0 && posT > pos && pos < bucleFiS && posT + n > bucleFiS;
                const long long p0 = torna && posT >= bucleFiS ? bucleIniS + (posT - bucleFiS) % llargB : posT;
                const int talla = torna && posT < bucleFiS ? (int) (bucleFiS - posT) : n;
                for (const auto& c : t.clips)
                    if (c.audio != nullptr)
                    {
                        sonaClip (fil, c, t.canals, p0, talla);
                        if (talla < n) { fil.escriuDes = talla; sonaClip (fil, c, t.canals, bucleIniS, n - talla); fil.escriuDes = 0; }
                    }
            }
            if (t.escoltaEntrada >= 0 && entrades != nullptr && mapaEnt != nullptr)
                for (int c = 0; c < t.canals; ++c)
                {
                    const int e = t.escoltaEntrada + c;
                    const int k = e < numMapaEnt ? mapaEnt[e] : -1;
                    if (k < 0 || k >= numEntrades || entrades[k] == nullptr) continue;
                    const float* x = entrades[k] + offsetEnt;
                    float* y = c == 0 ? xL : xR;
                    for (int i = 0; i < n; ++i) y[i] += x[i];
                }
        }
        if (! (t.canals == 2 || instrumentFet)) std::copy (xL, xL + n, xR);
        if (! t.inserts.empty()) efectes (t, t.inserts, t.scInserts, xL, xR, posT, n, ns, minim && f.enDirecte, &t.encesAmbRM, llindar);
        f.noValid = ! valid (xL, xR, n);
        if (instrumentFet) f.picEf = picDe (xL, xR, n);
        f.estat = Feina::Feta;
    }

    void Mesclador::processa (const Pla& pla, long long pos, int n, float* L, float* R, bool ambClic, Mesurador* vus,
                              bool reproduint, const EventDirecte* directe, int numDirecte) noexcept
    {
        if (n <= 0) return;
        margeAra = margeInici && reproduint;   // 0.71
        posReal = pos;                          // 0.98
        if (reproduint) margeInici = false;
        n = std::min (n, (int) pL.size());

        // 1.17: les sortides directes també (només les que van sonar el bloc d'abans).
        for (int c = 0; c < maxSortidesDirectes && usatsDir != 0; ++c)
            if ((usatsDir >> c) & 1u) std::fill_n (dirBuf.begin() + (std::ptrdiff_t) c * maxN, maxN, 0.0f);
        usatsDir = 0;
        // 0.33: el que arriba a cada bus en aquest bloc comença en silenci.
        const int nb = std::min (pla.numBusos, maxBusos);
        for (int k = 0; k < nb; ++k)
        {
            std::fill_n (busL.begin() + (std::ptrdiff_t) k * maxN, n, 0.0f);
            std::fill_n (busR.begin() + (std::ptrdiff_t) k * maxN, n, 0.0f);
        }
        // 0.56: i els de sidechain
        const int ns = std::min (pla.numSidechain, maxSidechain);
        for (int k = 0; k < ns; ++k)
        {
            std::fill_n (scL.begin() + (std::ptrdiff_t) k * maxN, n, 0.0f);
            std::fill_n (scR.begin() + (std::ptrdiff_t) k * maxN, n, 0.0f);
        }
        auto rampa = [n] (float* a, float g0, float g1) noexcept
        {
            if (std::abs (g0 - 1.0f) < 1.0e-7f && std::abs (g1 - 1.0f) < 1.0e-7f) return;
            const float d = (g1 - g0) / (float) n;
            for (int i = 0; i < n; ++i) a[i] *= g0 + d * (float) i;
        };

        const bool ambOrdre = pla.ordre.size() == pla.pistes.size();
        // 0.68: compensació del retard dels plugins (PDC). Cada pista llegeix el seu so (clips, notes i automatització)
        // tantes mostres abans com retarden els seus plugins, els del bus (o busos) on va i els del màster: així tot
        // arriba junt a la sortida, sense retardar res. El que es toca en directe no es pot avançar.
        const bool minim = retardMinim.load (std::memory_order_relaxed);
        const int llindar = llindarRM.load (std::memory_order_relaxed);
        int saltats = 0;
        const int latMaster = retardDe (pla.insertsMaster, minim, &saltats, &pla.encesAmbRMMaster, llindar);
        std::array<int, maxBusos> latBus {}, destiBus {};
        destiBus.fill (-1);
        for (const auto& t : pla.pistes)
            if (t.bus >= 0 && t.bus < nb && t.bus < maxBusos) { latBus[(size_t) t.bus] = retardDe (t.inserts, minim, &saltats, &t.encesAmbRM, llindar); destiBus[(size_t) t.bus] = t.desti; }
        std::array<int, maxBusos> latCami {};   // d'un bus fins a la sortida (busos dins de busos)
        std::array<bool, maxBusos> busDirecte {};   // 1.17: el camí acaba en una sortida directa (no passa pel màster)
        std::array<bool, maxBusos> directeBus {};
        for (const auto& t : pla.pistes)
            if (t.bus >= 0 && t.bus < nb && t.bus < maxBusos) directeBus[(size_t) t.bus] = t.sortidaDirecta >= 0;
        for (int b = 0; b < maxBusos && b < nb; ++b)
        {
            int l = 0, x = b, darrer = b;
            for (int k = 0; k < maxBusos && x >= 0 && x < nb; ++k) { l += latBus[(size_t) x]; darrer = x; x = destiBus[(size_t) x]; }
            latCami[(size_t) b] = l;
            busDirecte[(size_t) b] = directeBus[(size_t) darrer];
        }
        int pdcMaxim = 0;
        // 1.9: on llegeix cada pista (avançada pel retard del seu camí) i quines fan la feina pesada a part.
        auto posDe = [&] (const Pla::Pista& t, bool esBus, bool enDirecte) noexcept -> long long
        {
            if (! esBus)
            {
                // 0.81: armada i escoltant l'entrada: els efectes que ho saben fer (el Tune) passen en sec, sense retard
                const bool directeEntrada = t.escoltaEntrada >= 0;
                for (const auto& e : t.inserts) if (e != nullptr) e->posaDirecte (directeEntrada);
                const int av = (t.instrument != nullptr ? std::clamp (t.instrument->latencia(), 0, 1 << 20) : 0)
                             + retardDe (t.inserts, minim && enDirecte, &saltats, &t.encesAmbRM, llindar)
                             + (t.desti >= 0 && t.desti < nb && t.desti < maxBusos ? latCami[(size_t) t.desti] : 0)
                             + (t.desti >= 0 && t.desti < nb && t.desti < maxBusos ? (busDirecte[(size_t) t.desti] ? 0 : latMaster)
                                                                                   : (t.sortidaDirecta >= 0 ? 0 : latMaster));   // 1.17
                pdcMaxim = std::max (pdcMaxim, av);
                return pos + av;
            }
            if (t.bus < maxBusos)
                return pos + latCami[(size_t) t.bus] + (busDirecte[(size_t) t.bus] ? 0 : latMaster);   // 1.17   // l'automatització del bus, també quadrada
            return pos;
        };
        const int nf = (int) std::min (pla.pistes.size(), feines.size());
        {
            std::bitset<maxSlots> vist, repetit;   // dues pistes amb el mateix canal: millor al bucle, com sempre
            for (int q = 0; q < nf; ++q)
            {
                const int s = pla.pistes[ambOrdre ? (size_t) pla.ordre[(size_t) q] : (size_t) q].slot % maxSlots;
                if (vist[(size_t) s]) repetit[(size_t) s] = true;
                vist[(size_t) s] = true;
            }
            for (int q = 0; q < nf; ++q)
            {
                const auto& t = pla.pistes[ambOrdre ? (size_t) pla.ordre[(size_t) q] : (size_t) q];
                const int s = t.slot % maxSlots;
                auto& f = feines[(size_t) q];
                f.esBus = t.bus >= 0 && t.bus < nb;
                f.enDirecte = t.armada || s == slotDirecte;
                f.posT = posDe (t, f.esBus, f.enDirecte);
                f.estat = Feina::PerFer;
                f.instrumentFet = f.noValid = false;
                f.numEvs = 0;
                f.picInst = f.picEf = 0.0f;
                bool apart = ! f.esBus && q < maxSlots && ! repetit[(size_t) s] && ! slots.empty();
                for (int b : t.scInserts) apart = apart && ! (b >= 0 && b < ns);
                for (const auto& e : t.scEnvia) apart = apart && e.punt != 0;
                f.apart = apart;
            }
        }
        // 1.9: la feina pesada de les pistes «a part» (cada una a la seva memòria). 1.10: repartida entre els nuclis.
        {
            int numApart = 0;
            for (int q = 0; q < nf; ++q)
                if (feines[(size_t) q].apart) llistaApart[(size_t) numApart++] = q;
            const int nc = nuclis.load (std::memory_order_acquire);
            Equip* eq = nc > 1 ? equip.load (std::memory_order_acquire) : nullptr;
            long long treball = 0;   // 1.11: quant temps (de càlcul) han fet servir aquestes pistes
            if (eq != nullptr && numApart >= 2 && repartint)
            {
                bloc = { &pla, n, ns, llindar, numDirecte, reproduint, minim, directe };
                eq->treballNs.store (0, std::memory_order_relaxed);
                eq->fes (llistaApart.data(), numApart, std::min ({ nc - 1, eq->mida(), numApart - 1 }));
                treball = eq->treballNs.load (std::memory_order_relaxed);
            }
            else
            {
                const auto t0 = std::chrono::steady_clock::now();
                for (int k = 0; k < numApart; ++k)
                    calculaApart (fil0, pla, llistaApart[(size_t) k], n, ns, reproduint, minim, llindar, directe, numDirecte);
                treball = (long long) std::chrono::duration_cast<std::chrono::nanoseconds> (std::chrono::steady_clock::now() - t0).count();
            }
            // 1.11: Mibase decideix sol si cal repartir. La càrrega és un pic que puja de cop i baixa a poc a poc (la meitat
            // cada segon); es connecten els altres nuclis quan passa del llindar i es desconnecten quan baixa del 60 %.
            const float frac = (float) ((double) treball * 1.0e-9 * fs / std::max (1, n));
            carregaPic = std::max (frac, carregaPic * (float) std::exp2 (-(double) n / fs));
            const float ll = llindarNuclis.load (std::memory_order_relaxed);
            if (eq == nullptr || numApart < 2) repartint = false;
            else if (! repartint && carregaPic > ll) repartint = true;
            else if (repartint && carregaPic < 0.6f * ll) repartint = false;
            repartintAra.store (repartint, std::memory_order_relaxed);
            carregaPistes.store (carregaPic, std::memory_order_relaxed);
        }

        for (size_t q = 0; q < pla.pistes.size(); ++q)
        {
            const auto& t = pla.pistes[ambOrdre ? (size_t) pla.ordre[q] : q];
            const int s = t.slot % maxSlots;
            const Feina* fa = q < (size_t) nf ? &feines[q] : nullptr;
            const bool apart = fa != nullptr && fa->apart;
            const bool esBus = fa != nullptr ? fa->esBus : (t.bus >= 0 && t.bus < nb);
            float* const xL = apart ? apartL.data() + (std::ptrdiff_t) q * maxN : esBus ? busL.data() + (std::ptrdiff_t) t.bus * maxN : pL.data();
            float* const xR = apart ? apartR.data() + (std::ptrdiff_t) q * maxN : esBus ? busR.data() + (std::ptrdiff_t) t.bus * maxN : pR.data();
            bool instrumentFet = false;
            const bool enDirecte = fa != nullptr ? fa->enDirecte : (t.armada || s == slotDirecte);
            const long long posT = fa != nullptr ? fa->posT : posDe (t, esBus, enDirecte);   // 0.68
            if (apart)
            {
                if (t.instrument != nullptr && ! reproduint && fa->numEvs > 0) sonda.evsAturat.fetch_add ((unsigned) fa->numEvs, std::memory_order_relaxed);
                if (fa->estat == Feina::SaltaAbans)
                    continue;
                instrumentFet = fa->instrumentFet;
                if (instrumentFet) puja (sonda.picInstrument, fa->picInst, n, fs);
            }
            else if (esBus) {}   // un bus es calcula sempre (també aturat: les cues dels efectes no es tallen)
            else if (t.instrument != nullptr && ! slots.empty())
            {
                // 0.56: paràmetres automatitzats de l'instrument (al començament del bloc)
                for (const auto& a : t.autoParams)
                    if (a.instrument && ! (a.lliure != nullptr && a.lliure->load (std::memory_order_relaxed)))
                    {
                        const float x = a.corba.a ((double) (posT + n / 2) / fs);
                        if (std::abs (x - a.ultim) > 1.0e-6f) { a.obj->posaParametre (a.index, x); a.ultim = x; }
                    }
                // L'instrument sempre rep els seus missatges (també silenciat: així les notes s'apaguen).
                instrumentPista (fil0, t, posT, n, reproduint, directe, numDirecte);
                if (! reproduint && fil0.numEvs > 0) sonda.evsAturat.fetch_add ((unsigned) fil0.numEvs, std::memory_order_relaxed);
                if (fil0.numEvs == 0 && ! t.instrument->fentSo())
                    continue;
                t.instrument->processa (fil0.evs.data(), fil0.numEvs, pL.data(), pR.data(), n);
                instrumentFet = true;
                puja (sonda.picInstrument, picDe (pL.data(), pR.data(), n), n, fs);
            }
            else if (! reproduint && t.escoltaEntrada < 0)
                continue;   // aturat: només sonen els instruments (i les pistes que s'escolten, 0.71)

            const Guanys gu = guanysDe (t, posT, n);
            const float porta = gu.porta, fader = gu.fader, panL = gu.panL, panR = gu.panR, obert = gu.obert;
            // 1.5.2: perquè el fader i el pom de la pista es moguin amb l'automatització
            volAuto[(size_t) s].store (gu.llegeixVol ? 1.0f + (float) t.autoVolum.a (gu.segAuto) : 0.0f, std::memory_order_relaxed);
            panAuto[(size_t) s].store (gu.llegeixPan ? 1.0f + (float) t.autoPan.a (gu.segAuto) : 0.0f, std::memory_order_relaxed);
            if (! gFet[(size_t) s])
            {
                gPorta[(size_t) s] = porta; gFader[(size_t) s] = fader; gPanL[(size_t) s] = panL; gPanR[(size_t) s] = panR;
                gObert[(size_t) s] = obert; gFet[(size_t) s] = true;
            }
            const float porta0 = gPorta[(size_t) s], fader0 = gFader[(size_t) s], panL0 = gPanL[(size_t) s],
                        panR0 = gPanR[(size_t) s], obert0 = gObert[(size_t) s];
            gPorta[(size_t) s] = porta; gFader[(size_t) s] = fader; gPanL[(size_t) s] = panL; gPanR[(size_t) s] = panR;
            gObert[(size_t) s] = obert;
            if (porta0 < 1.0e-6f && porta < 1.0e-6f && ! t.scSempre)
                continue;   // no sona: ni es calcula (0.56: tret que alimenti un sidechain)
            bool hiHaPre = t.scSempre;
            for (const auto& e : t.envia) hiHaPre = hiHaPre || e.mode == 0;
            if (! hiHaPre && fader0 < 1.0e-6f && fader < 1.0e-6f)
                continue;   // fader a baix i cap enviament pre-fader

            if (apart)
            {
                // 1.9: l'instrument, els clips, l'entrada i els efectes ja estan fets (calculaApart)
                if (fa->noValid)
                {
                    sonda.noValids.fetch_add (1, std::memory_order_relaxed);
                    sonda.slotNoValid.store (t.slot, std::memory_order_relaxed);
                    if (t.instrument != nullptr) for (auto& sl : slots) if (sl.inst == t.instrument.get()) sl.apagar = true;
                }
                if (instrumentFet) puja (sonda.picEfectes, fa->picEf, n, fs);
            }
            else
            {
            if (! instrumentFet && ! esBus)
            {
                std::fill (pL.begin(), pL.begin() + n, 0.0f);
                if (t.canals == 2) std::fill (pR.begin(), pR.begin() + n, 0.0f);
                fil0.oL = pL.data(); fil0.oR = pR.data();
                if (reproduint && ! nomesCua)
                {
                    // 0.98: llegint avançat (PDC) amb el bucle: el tros que passa del final es llegeix de l'inici
                    const long long llargB = bucleFiS - bucleIniS;
                    const bool torna = llargB > 0 && posT > pos && pos < bucleFiS && posT + n > bucleFiS;
                    const long long p0 = torna && posT >= bucleFiS ? bucleIniS + (posT - bucleFiS) % llargB : posT;
                    const int talla = torna && posT < bucleFiS ? (int) (bucleFiS - posT) : n;
                    for (const auto& c : t.clips)
                        if (c.audio != nullptr)
                        {
                            sonaClip (fil0, c, t.canals, p0, talla);
                            if (talla < n) { fil0.escriuDes = talla; sonaClip (fil0, c, t.canals, bucleIniS, n - talla); fil0.escriuDes = 0; }
                        }
                }
                // 0.71: el que entra per la targeta (la guitarra, la veu), amb els efectes i el fader de la pista
                if (t.escoltaEntrada >= 0 && entrades != nullptr && mapaEnt != nullptr)
                    for (int c = 0; c < t.canals; ++c)
                    {
                        const int e = t.escoltaEntrada + c;
                        const int k = e < numMapaEnt ? mapaEnt[e] : -1;
                        if (k < 0 || k >= numEntrades || entrades[k] == nullptr) continue;
                        const float* x = entrades[k] + offsetEnt;
                        float* y = c == 0 ? pL.data() : pR.data();
                        for (int i = 0; i < n; ++i) y[i] += x[i];
                    }
            }

            // Una pista mono passa als dos costats (al mig): a partir d'aquí tot és estèreo.
            if (! (t.canals == 2 || instrumentFet || esBus)) std::copy (xL, xL + n, xR);
            // 0.19: efectes de la pista (abans del fader). 0.56: amb sidechain i automatització.
            enviaSc (t, 0, xL, xR, n, ns);        // sidechain: abans dels efectes
            if (! t.inserts.empty()) efectes (t, t.inserts, t.scInserts, xL, xR, posT, n, ns, minim && (esBus || enDirecte), &t.encesAmbRM, llindar);
            if (! valid (xL, xR, n))   // 0.64: un efecte o instrument espatllat no pot deixar muda tota la mescla
            {
                sonda.noValids.fetch_add (1, std::memory_order_relaxed);
                sonda.slotNoValid.store (t.slot, std::memory_order_relaxed);
                if (t.instrument != nullptr) for (auto& sl : slots) if (sl.inst == t.instrument.get()) sl.apagar = true;
            }
            if (instrumentFet) puja (sonda.picEfectes, picDe (xL, xR, n), n, fs);
            }
            enviaSc (t, 1, xL, xR, n, ns);        // sidechain: després dels efectes (abans del fader)

            rampa (xL, porta0, porta);
            rampa (xR, porta0, porta);
            envia (t, 0, xL, xR, n, nb);          // pre-fader
            rampa (xL, fader0, fader);
            rampa (xR, fader0, fader);
            envia (t, 1, xL, xR, n, nb);          // post-fader (abans del panorama)
            enviaSc (t, 2, xL, xR, n, ns);        // sidechain: després del fader
            rampa (xL, panL0, panL);
            rampa (xR, panR0, panR);
            envia (t, 2, xL, xR, n, nb);          // post-pan

            if (vus != nullptr)
            {
                vus[s * 2].processa (xL, n);
                vus[s * 2 + 1].processa (xR, n);
            }

            // Cap al bus on va, o a la sortida.
            if (obert0 < 1.0e-6f && obert < 1.0e-6f) continue;
            const bool aBus = t.desti >= 0 && t.desti < nb;
            if (! aBus && t.sortidaDirecta >= 0 && t.sortidaDirecta < maxSortidesDirectes && ! dirBuf.empty())
            {
                // 1.17: directa a la targeta (sense passar pel màster)
                const float dob = (obert - obert0) / (float) n;
                const int c0 = t.sortidaDirecta;
                if (t.canalsDirecta == 1)
                {
                    float* const d = dirBuf.data() + (std::ptrdiff_t) c0 * maxN;
                    for (int i = 0; i < n; ++i) d[i] += 0.5f * (xL[i] + xR[i]) * (obert0 + dob * (float) i);
                    usatsDir |= (std::uint64_t) 1 << c0;
                }
                else
                {
                    const int c1 = std::min (c0 + 1, maxSortidesDirectes - 1);
                    float* const d0 = dirBuf.data() + (std::ptrdiff_t) c0 * maxN;
                    float* const d1 = dirBuf.data() + (std::ptrdiff_t) c1 * maxN;
                    for (int i = 0; i < n; ++i)
                    {
                        const float g = obert0 + dob * (float) i;
                        d0[i] += xL[i] * g;
                        if (c1 != c0) d1[i] += xR[i] * g;
                    }
                    usatsDir |= ((std::uint64_t) 1 << c0) | ((std::uint64_t) 1 << c1);
                }
                continue;
            }
            float* const dL = aBus ? busL.data() + (std::ptrdiff_t) t.desti * maxN : L;
            float* const dR = aBus ? busR.data() + (std::ptrdiff_t) t.desti * maxN : R;
            const float dob = (obert - obert0) / (float) n;
            for (int i = 0; i < n; ++i)
            {
                const float g = obert0 + dob * (float) i;
                dL[i] += xL[i] * g;
                dR[i] += xR[i] * g;
            }
        }

        sonda.pdcMaxim.store (pdcMaxim, std::memory_order_relaxed);   // 0.68
        sonda.pdcSaltats.store (saltats, std::memory_order_relaxed);
        // 0.19: cadena de màstering (abans del volum del màster). 0.56: amb sidechain.
        if (! pla.insertsMaster.empty())
        {
            efectes (pistaBuida, pla.insertsMaster, pla.scMaster, L, R, pos, n, ns, minim, &pla.encesAmbRMMaster, llindar);
        }
        if (! valid (L, R, n))   // 0.64
        {
            sonda.noValids.fetch_add (1, std::memory_order_relaxed);
            sonda.slotNoValid.store (-2, std::memory_order_relaxed);
            gFet.fill (false);
            masterFet = false;
        }
        puja (sonda.picMaster, picDe (L, R, n), n, fs);

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
        preMida = (long long) std::ceil (segonsPregravacio * fs);   // 0.72
        preL.assign ((size_t) preMida, 0.0f);
        preR.assign ((size_t) preMida, 0.0f);
        preEscriu = preContinus = 0;
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

    void MotorEstudi::setMapaSortides (const std::vector<int>& mapa)
    {
        mapaSortides.fill (-1);
        for (size_t i = 0; i < std::min (mapa.size(), mapaSortides.size()); ++i)
            mapaSortides[i] = mapa[i];
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
    void MotorEstudi::entra (int entrada, int canals) { cuaOrdres.posa ({ Ordre::Tipus::Entra, 0.0, 0.0, entrada, canals }); }
    void MotorEstudi::surt()                          { cuaOrdres.posa ({ Ordre::Tipus::Surt, 0.0, 0.0, 0, 0 }); }

    void MotorEstudi::guardaPregravacio (const float* const* entrades, int numEntrades, int fet, int m) noexcept
    {
        const int e = preEntrada.load (std::memory_order_relaxed);
        const int c = preCanals.load (std::memory_order_relaxed);
        if (e != preEntradaVista || c != preCanalsVists) { preEntradaVista = e; preCanalsVists = c; preContinus = 0; }
        if (e < 0 || preMida <= 0) return;
        auto canal = [&] (int k) -> const float*
        {
            const int x = mapaEntrades[(size_t) std::clamp (k, 0, maxEntrades - 1)];
            return (x >= 0 && x < numEntrades && entrades[x] != nullptr) ? entrades[x] + fet : nullptr;
        };
        const float* a = canal (e);
        const float* b = c == 2 ? canal (e + 1) : a;
        if (b == nullptr) b = a;
        for (int i = 0; i < m; ++i)
        {
            preL[(size_t) preEscriu] = a != nullptr ? a[i] : 0.0f;
            preR[(size_t) preEscriu] = b != nullptr ? b[i] : 0.0f;
            if (++preEscriu >= preMida) preEscriu = 0;
        }
        preContinus += m;
    }

    void MotorEstudi::escriuPregravacio (long long frames) noexcept
    {
        // Els «frames» darrers de l'anell, del més vell al més nou (en dos trossos si dona la volta).
        long long ini = preEscriu - frames;
        if (ini < 0) ini += preMida;
        while (frames > 0)
        {
            const long long tros = std::min (frames, preMida - ini);
            const float* canals[2] { preL.data() + ini, preR.data() + ini };
            grav.escriu (canals, (int) tros);
            frames -= tros;
            ini = 0;
        }
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
                    preContinus = 0;
                    mesclador.reinicia();
                    estat.store ((int) Estat::Sonant, std::memory_order_release);
                    break;
                case Ordre::Tipus::Atura:
                    if ((Estat) estat.load() == Estat::Gravant) fiGravacio.store ((double) pos / fs, std::memory_order_release);
                    preContinus = 0;
                    estat.store ((int) Estat::Aturat, std::memory_order_release);
                    mesclador.apagaNotes();
                    break;
                case Ordre::Tipus::Situa:
                    if ((Estat) estat.load() == Estat::Sonant)
                    {
                        pos = (long long) std::llround (o.t * fs);
                        entraARuta();
                        compteFins = std::min (compteFins, pos);
                        preContinus = 0;
                        mesclador.reinicia();
                    }
                    break;
                case Ordre::Tipus::Grava:
                    pos = (long long) std::llround (o.t * fs);
                    compteFins = (long long) std::llround (o.t2 * fs);
                    entradaGrava = std::clamp (o.a, 0, maxEntrades - 1);
                    canalsGrava = o.b == 2 ? 2 : 1;
                    tramRuta = -1;   // gravant, sempre lineal
                    preContinus = 0;
                    iniciFitxer.store (o.t, std::memory_order_release);
                    puntGravacio.store (o.t2, std::memory_order_release);
                    mostresFitxer = 0;   // 0.81
                    voltesGrav.store (0, std::memory_order_release);
                    primeraVolta.store (-1.0, std::memory_order_release);
                    mesclador.setSilenci ({});
                    mesclador.reinicia();
                    estat.store ((int) (pos < compteFins ? Estat::Comptant : Estat::Gravant), std::memory_order_release);
                    break;
                case Ordre::Tipus::Entra:
                {
                    // 0.72 Punch In: només si està sonant. No es mou el capçal ni es reinicia res (no es talla el so).
                    if ((Estat) estat.load() != Estat::Sonant) break;
                    const bool audio = o.a >= 0;
                    entradaGrava = std::clamp (o.a, 0, maxEntrades - 1);
                    canalsGrava = o.b == 2 ? 2 : 1;
                    long long previ = 0;
                    if (audio && grav.activa() && preEntradaVista == o.a && preCanalsVists == canalsGrava)
                        previ = std::min (preContinus, preMida);
                    if (previ > 0) escriuPregravacio (previ);
                    mostresFitxer = audio ? previ : 0;   // 0.81
                    voltesGrav.store (0, std::memory_order_release);
                    primeraVolta.store (-1.0, std::memory_order_release);
                    if (tramRuta >= 0) { tramRuta = -1; mesclador.setSilenci ({}); }   // gravant, sempre lineal
                    compteFins = pos;
                    iniciFitxer.store ((double) (pos - previ) / fs, std::memory_order_release);
                    puntGravacio.store ((double) pos / fs, std::memory_order_release);
                    estat.store ((int) Estat::Gravant, std::memory_order_release);
                    break;
                }
                case Ordre::Tipus::Surt:
                {
                    // 0.72 Punch Out: deixa de gravar i continua sonant (el bucle torna a valer si encara no s'ha passat).
                    const auto e = (Estat) estat.load();
                    if (e != Estat::Gravant && e != Estat::Comptant) break;
                    fiGravacio.store ((double) pos / fs, std::memory_order_release);
                    compteFins = std::min (compteFins, pos);   // sense claqueta forçada
                    estat.store ((int) Estat::Sonant, std::memory_order_release);
                    break;
                }
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
        ambPla.store (pla != nullptr, std::memory_order_relaxed);
        if (pla == nullptr)
            return;
        if (ara == Estat::Aturat)
        {
            // Aturat: només els instruments (el que es toca en directe i les cues de les notes).
            mesclador.setSlotDirecte (slotTeclat.load (std::memory_order_relaxed));   // 0.68
            int fet = 0;
            while (fet < n)
            {
                const int m = std::min (n - fet, maxBloc);
                std::fill (mixL.begin(), mixL.begin() + m, 0.0f);
                std::fill (mixR.begin(), mixR.begin() + m, 0.0f);
                mesclador.setEntrades (entrades, numEntrades, mapaEntrades.data(), maxEntrades, fet);   // 0.71
                mesclador.processa (*pla, pos, m, mixL.data(), mixR.data(), false, vus.data(), false,
                                    fet == 0 ? directeBloc.data() : nullptr, fet == 0 ? numDirecte : 0);
                sumaSortida (sortides, numSortides, fet, m);
                fet += m;
            }
            return;
        }

        mesclador.setSlotDirecte (slotTeclat.load (std::memory_order_relaxed));   // 0.68: per a «Retard mínim»
        const bool gravant = ara == Estat::Comptant || ara == Estat::Gravant;
        int fet = 0;
        while (fet < n)
        {
            int m = std::min (n - fet, maxBloc);
            // El bucle (no mentre es grava): el tros acaba al final del bucle i torna a l'inici.
            const long long fiBucle = (long long) std::llround (pla->bucleFi * fs);
            // 0.81: gravant àudio (no durant el compte d'entrada), també torna: cada volta serà una presa
            const bool gravaBucle = gravant && ara == Estat::Gravant && grav.activa();
            const bool potTornar = pla->bucle && (! gravant || gravaBucle) && pos < fiBucle;
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
            mesclador.setEntrades (entrades, numEntrades, mapaEntrades.data(), maxEntrades, fet);   // 0.71
            if (potTornar) mesclador.setBucle ((long long) std::llround (pla->bucleIni * fs), fiBucle);   // 0.98
            else mesclador.setBucle (-1, -1);
            mesclador.processa (*pla, pos, m, mixL.data(), mixR.data(), ambClic, vus.data(), true,
                                fet == 0 ? directeBloc.data() : nullptr, fet == 0 ? numDirecte : 0);

            guardaPregravacio (entrades, numEntrades, fet, m);   // 0.72
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
                mostresFitxer += m;   // 0.81
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
                preContinus = 0;
                mesclador.setSilenci (pla->ruta[(size_t) tramRuta].silenci);
                mesclador.reiniciaBucle();
                continue;
            }
            if (potTornar && pos >= fiBucle)
            {
                if (gravaBucle)   // 0.81: una volta més de la gravació en bucle
                {
                    const long long ini = (long long) std::llround (pla->bucleIni * fs);
                    if (voltesGrav.load (std::memory_order_relaxed) == 0)
                    {
                        primeraVolta.store ((double) mostresFitxer / fs, std::memory_order_release);
                        llargVolta.store ((double) (fiBucle - ini) / fs, std::memory_order_release);
                        iniciVolta.store ((double) ini / fs, std::memory_order_release);
                    }
                    voltesGrav.fetch_add (1, std::memory_order_acq_rel);
                }
                pos = (long long) std::llround (pla->bucleIni * fs);
                preContinus = 0;
                mesclador.reiniciaBucle();
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

        // 1.17: les sortides directes de les pistes i els busos, cada una pel seu canal de la targeta (si és encès)
        const auto usats = mesclador.usatsDirecte();
        for (int c = 0; c < maxSortidesDirectes && usats != 0; ++c)
        {
            if (((usats >> c) & 1u) == 0) continue;
            const int k = mapaSortides[(size_t) c];
            if (k < 0 || k >= numSortides || sortides[k] == nullptr) continue;
            const float* d = mesclador.canalDirecte (c);
            for (int i = 0; i < m; ++i) sortides[k][fet + i] = std::clamp (sortides[k][fet + i] + d[i], -1.0f, 1.0f);
        }
    }

    //==========================================================================
    std::pair<double, double> trosPerExportar (const Projecte& p)
    {
        if (p.bucle.marcat && p.bucle.actiu && p.bucle.a > p.bucle.de)
        {
            return { edicio::iniciBucle (p), edicio::fiBucle (p) };
        }
        return limitsCanco (p);   // 1.18: amb les banderoles d'inici i final
    }

    std::pair<double, double> limitsCanco (const Projecte& p)
    {
        auto [a0, b0] = limitsClips (p);
        double a = p.iniciCanco >= 0.0 ? p.iniciCanco : a0;
        double b = p.fiCanco >= 0.0 ? p.fiCanco : b0;
        if (p.iniciCanco < 0.0 && p.fiCanco < 0.0 && b0 <= a0) return { 0.0, 0.0 };
        if (b <= a) b = a + 1.0;   // una banderola posada i res que soni: com a mínim un segon
        return { std::max (0.0, a), b };
    }

    std::pair<double, double> limitsClips (const Projecte& p)
    {
        const auto solo = edicio::estatSolo (p);
        double a = 1.0e18, b = -1.0e18;
        for (size_t it = 0; it < p.pistes.size(); ++it)
        {
            const auto& t = p.pistes[it];
            if (solo[it] == 0) continue;
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
                const double ini = c.pos + std::max (c.inici, 0.0), fi = c.pos + std::min (c.fi, elastic::duradaClip (c, s->durada));
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

    double duradaExportacio (const Pla& pla, double desDe, double finsA)
    {
        if (pla.ruta.empty()) return std::max (0.0, finsA - desDe);
        double d = 0.0;
        for (const auto& t : pla.ruta) d += std::max (0.0, t.fi - t.ini);
        return d;
    }

    ComprovacioExport comprovaExport (const float* x, long long frames, double fs, int bits)
    {
        ComprovacioExport c;
        c.bits = bits;
        c.dither = bits == 16;
        if (x == nullptr || frames <= 0 || fs <= 0.0) return c;
        c.segons = (double) frames / fs;
        auto nom = [] (double v, int d = 1) { char b[32]; std::snprintf (b, sizeof b, "%.*f", d, v); std::string s (b); for (auto& ch : s) if (ch == '.') ch = ','; return s; };
        // silencis (per sota de -60 dBFS)
        const float llindar = 0.001f;
        long long a = 0, b = frames - 1;
        while (a < frames && std::abs (x[2 * a]) < llindar && std::abs (x[2 * a + 1]) < llindar) ++a;
        while (b > a && std::abs (x[2 * b]) < llindar && std::abs (x[2 * b + 1]) < llindar) --b;
        c.silenciInici = (double) a / fs;
        c.silenciFinal = (double) (frames - 1 - b) / fs;
        // de cop: el so comença o s'acaba ja fort (els primers o els últims 5 ms passen de -30 dBFS)
        const long long w = std::max<long long> (1, (long long) (0.005 * fs));
        float pi = 0.0f, pf = 0.0f;
        for (long long i = 0; i < std::min (w, frames); ++i) pi = std::max (pi, std::max (std::abs (x[2 * i]), std::abs (x[2 * i + 1])));
        for (long long i = std::max (0LL, frames - w); i < frames; ++i) pf = std::max (pf, std::max (std::abs (x[2 * i]), std::abs (x[2 * i + 1])));
        c.deCopInici = pi > 0.0316f;
        c.deCopFinal = pf > 0.01f;
        // loudness, pics i clips
        std::vector<float> L ((size_t) frames), R ((size_t) frames);
        for (long long i = 0; i < frames; ++i) { L[(size_t) i] = x[2 * i]; R[(size_t) i] = x[2 * i + 1]; }
        const auto r = loudness::analitza (L.data(), R.data(), (size_t) frames, fs);
        c.truePeak = r.truePeakMax; c.samplePeak = r.samplePeakMax; c.integrated = r.integrated; c.lra = r.lra; c.clips = r.clips;
        // frases
        if (c.silenciInici > 2.0) c.avisos.push_back ("Comença amb " + nom (c.silenciInici) + " s de silenci: potser cal retallar-lo.");
        else c.be.push_back ("Inici: " + nom (c.silenciInici, 2) + " s de silenci.");
        if (c.deCopInici && c.silenciInici < 0.001) c.avisos.push_back ("Comença de cop (el so ja és fort al primer instant): si no és volgut, posa-hi un fade d'entrada curt.");
        if (c.silenciFinal > 3.0) c.avisos.push_back ("S'acaba amb " + nom (c.silenciFinal) + " s de silenci.");
        if (c.deCopFinal) c.avisos.push_back ("S'acaba de cop (encara sona a l'últim instant): posa-hi un fade de sortida o deixa més cua.");
        else c.be.push_back ("Final net (s'apaga sol).");
        if (c.clips > 0) c.avisos.push_back ("Hi ha " + std::to_string (c.clips) + " mostres a 0 dBFS o més: es retallaran al fitxer. Baixa el sostre del limitador.");
        if (c.truePeak > -0.1f) c.avisos.push_back ("True peak de " + nom (c.truePeak) + " dBTP: pot distorsionar en passar a MP3/AAC. Millor -1 dBTP.");
        else if (c.truePeak > -1.0f) c.avisos.push_back ("True peak de " + nom (c.truePeak) + " dBTP: per a plataformes de streaming, millor -1 dBTP o menys (per a CD, d'acord).");
        else c.be.push_back ("True peak " + nom (c.truePeak) + " dBTP.");
        if (c.integrated > -70.0f) c.be.push_back ("Loudness " + nom (c.integrated) + " LUFS, LRA " + nom (c.lra) + " LU.");
        c.be.push_back (c.dither ? "Dither triangular aplicat una sola vegada, al final (16 bits)." : "Sense dither: a " + std::to_string (bits) + " bits no cal.");
        return c;
    }

    bool exportaPla (const Pla& pla, double fs, double desDe, double finsA, int bits, double cua, const std::filesystem::path& desti,
                     std::string& error, const std::function<void (double)>& progres, bool retallaCua, ComprovacioExport* comprovacio)
    {
        struct Tros { long long ini, fi; std::bitset<128> silenci; };
        std::vector<Tros> trossos;
        if (! pla.ruta.empty())
            for (const auto& t : pla.ruta)
            {
                const long long a = (long long) std::llround (t.ini * fs), b = (long long) std::llround (t.fi * fs);
                if (b > a) trossos.push_back ({ a, b, t.silenci });
            }
        else if (finsA > desDe)
            trossos.push_back ({ (long long) std::llround (desDe * fs), (long long) std::llround (finsA * fs), {} });
        if (trossos.empty())
        {
            error = "No hi ha res que soni per exportar.";
            return false;
        }
        const long long llargCua = (long long) std::llround (std::max (0.0, cua) * fs);
        long long total = llargCua;
        for (auto& t : trossos) total += t.fi - t.ini;
        const long long minim = total - llargCua;   // mai es talla abans d'aquí

        constexpr int bloc = 4096;
        Mesclador m;
        m.prepara (fs, bloc);
        std::vector<float> L (bloc), R (bloc);
        std::vector<float> tot;   // tot en memòria per poder treure el silenci del final (una cançó de 5 min: ~100 MB)
        tot.reserve ((size_t) (total * 2));
        for (size_t k = 0; k < trossos.size(); ++k)
        {
            const auto& t = trossos[k];
            m.setSilenci (t.silenci);
            if (k > 0) m.reiniciaBucle();
            const long long fi = t.fi + (k + 1 == trossos.size() ? llargCua : 0);
            for (long long p = t.ini; p < fi; p += bloc)
            {
                const int n = (int) std::min ((long long) bloc, std::min (fi, p < t.fi ? t.fi : fi) - p);   // el bloc no passa del final
                m.setNomesCua (p >= t.fi);   // després del final: només s'apaguen els efectes
                std::fill (L.begin(), L.end(), 0.0f);
                std::fill (R.begin(), R.end(), 0.0f);
                m.processa (pla, p, n, L.data(), R.data(), false, nullptr);
                for (int i = 0; i < n; ++i) { tot.push_back (L[(size_t) i]); tot.push_back (R[(size_t) i]); }
                if (progres) progres ((double) (tot.size() / 2) / (double) total);
            }
        }
        // La cua: fins on encara se sent (-90 dB), més 50 ms
        long long frames = (long long) (tot.size() / 2);
        m.setNomesCua (false);
        while (retallaCua && frames > minim && std::abs (tot[(size_t) (2 * frames - 1)]) < 3.0e-5f && std::abs (tot[(size_t) (2 * frames - 2)]) < 3.0e-5f) --frames;
        if (retallaCua) frames = std::min ((long long) (tot.size() / 2), frames + (long long) (0.05 * fs));
        if (comprovacio != nullptr) *comprovacio = comprovaExport (tot.data(), frames, fs, bits);   // 1.3: abans del dither
        if (bits == 16)
        {
            // Dither triangular (TPDF) d'1 LSB: sense distorsió als finals que s'apaguen
            std::uint32_t llavor = 22222u;
            auto aleatori = [&llavor] { llavor = llavor * 1664525u + 1013904223u; return (float) (llavor >> 8) / 16777216.0f; };
            for (long long i = 0; i < frames * 2; ++i) tot[(size_t) i] += (aleatori() - aleatori()) / 32768.0f;
        }
        EscriptorWav wav;
        if (! wav.obre (desti, 2, fs, bits == 16 ? 16 : 24))
        {
            error = "No s'ha pogut crear el fitxer.";
            return false;
        }
        if (! wav.escriu (tot.data(), frames) || ! wav.tanca())
        {
            error = "No s'ha pogut escriure al disc.";
            return false;
        }
        return true;
    }

    void renderitzaPla (const Pla& pla, double fs, double desDe, double finsA, std::vector<float>& L, std::vector<float>& R,
                        const std::function<void (double)>& progres)
    {
        L.clear(); R.clear();
        struct Tros { long long ini, fi; std::bitset<128> silenci; };
        std::vector<Tros> trossos;
        if (! pla.ruta.empty())
            for (const auto& t : pla.ruta)
            {
                const long long a = (long long) std::llround (t.ini * fs), b = (long long) std::llround (t.fi * fs);
                if (b > a) trossos.push_back ({ a, b, t.silenci });
            }
        else if (finsA > desDe)
            trossos.push_back ({ (long long) std::llround (desDe * fs), (long long) std::llround (finsA * fs), {} });
        long long total = 0;
        for (auto& t : trossos) total += t.fi - t.ini;
        if (total <= 0) return;
        L.reserve ((size_t) total); R.reserve ((size_t) total);
        constexpr int bloc = 4096;
        Mesclador m;
        m.prepara (fs, bloc);
        std::vector<float> bl (bloc), br (bloc);
        for (size_t k = 0; k < trossos.size(); ++k)
        {
            const auto& t = trossos[k];
            m.setSilenci (t.silenci);
            if (k > 0) m.reiniciaBucle();
            for (long long p = t.ini; p < t.fi; p += bloc)
            {
                const int n = (int) std::min ((long long) bloc, t.fi - p);
                std::fill (bl.begin(), bl.end(), 0.0f);
                std::fill (br.begin(), br.end(), 0.0f);
                m.processa (pla, p, n, bl.data(), br.data(), false, nullptr);
                L.insert (L.end(), bl.begin(), bl.begin() + n);
                R.insert (R.end(), br.begin(), br.begin() + n);
                if (progres) progres ((double) L.size() / (double) total);
            }
        }
    }

    Projecte projectePerExportar (const Projecte& p, const OpcionsExport& o, const std::vector<int>& pistes, std::pair<double, double>& tros)
    {
        Projecte q = p;
        q.clic = false;
        if (o.tros == OpcionsExport::Canco)
        {
            q.estructuraActiva = o.estructura;
            if (! o.estructura.empty() && edicio::estructuraActiva (q) == nullptr) q.estructuraActiva.clear();
            q.bucle.actiu = false;
        }
        else
        {
            q.estructuraActiva.clear();
            if (o.tros == OpcionsExport::Part) q.bucle.actiu = false;
        }
        if (! pistes.empty())
        {
            // Només sonen les triades (com si estiguessin en solo: els busos que alimenten també)
            for (auto& t : q.pistes) t.solo = false;
            for (int i : pistes)
                if (i >= 0 && i < (int) q.pistes.size()) { q.pistes[(size_t) i].solo = true; q.pistes[(size_t) i].mut = false; }
        }
        if (! o.efectes)
            for (auto& t : q.pistes)
            {
                std::erase_if (t.inserts, [] (const Insert& x) { return ! esInsertVeu (x); });   // el Vocal Editor no és un efecte
                t.enviaments.clear();
            }
        if (! o.master) q.insertsMaster.clear();

        tros = { 0.0, 0.0 };
        if (o.tros == OpcionsExport::Bucle)
        {
            if (q.bucle.marcat && q.bucle.a > q.bucle.de) tros = { edicio::iniciBucle (q), edicio::fiBucle (q) };
        }
        else if (o.tros == OpcionsExport::Part)
        {
            for (const auto& pt : q.parts)
                if (pt.id == o.part && pt.a > pt.de) tros = { edicio::segonsDeCompas (q, pt.de), edicio::segonsDeCompas (q, pt.a) };
        }
        else
            tros = limitsCanco (q);   // 1.18: «Tota la cançó» = de la banderola d'inici a la de final
        return q;
    }
}
