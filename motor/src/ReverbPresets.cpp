#include "mibase/Reverb.h"
#include "mibase/ProjecteDisc.h"

#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <utility>

namespace mibase::reverb
{
    namespace
    {
        using V = std::initializer_list<std::pair<int, float>>;
        struct Def { const char* nom; int cat; const char* desc; int alg; V v; };

        const std::vector<std::string> cats = { "Veus", "Guitarres", "Bateria i percussió", "Baix", "Piano i teclats",
                                                "Sintes i pads", "Espais acústics", "Ambient i cinema", "Creatius i FX" };
        enum { CVeu, CGui, CBat, CBaix, CPiano, CSinte, CEspai, CAmb, CFx };

        // Notes (índex de numNotes): 1/16 = 3, 1/8T = 5, 1/8 = 6, 1/8· = 7, 1/4 = 9, 1/2 = 11, 1/1 = 12, 2/1 = 13.
        const Def defs[] = {
            // ---------------- VEUS (15) ----------------
            { "Vocal Studio", CVeu, "Placa curta i neta per a una veu principal: dona cos sense allunyar-la.", Plate,
              { { Mix, 18 }, { Decay, 1.4f }, { PreDelay, 25 }, { Size, 45 }, { Damping, 45 }, { Diffusion, 80 }, { LowCut, 180 }, { HighCut, 11000 }, { DecGreus, 0.7f }, { DecAguts, 0.75f }, { ModDepth, 20 } } },
            { "Intimate Vocal", CVeu, "Habitació petita i càlida: la veu sona a prop, com en una sessió acústica.", Room,
              { { Mix, 14 }, { Decay, 0.8f }, { PreDelay, 10 }, { Size, 30 }, { Damping, 55 }, { EarlyLate, 45 }, { LowCut, 200 }, { HighCut, 9000 }, { Width, 80 } } },
            { "Big Indie Vocal", CVeu, "Sala gran i ampla amb ducking: la cua s'obre als espais entre frases.", Hall,
              { { Mix, 24 }, { Decay, 3.2f }, { PreDelay, 45 }, { Size, 75 }, { Damping, 40 }, { ModDepth, 35 }, { LowCut, 220 }, { Width, 120 }, { DuckOn, 1 }, { DuckThr, -28 }, { DuckAmt, 6 }, { DuckRel, 350 } } },
            { "Vocal Plate", CVeu, "La placa clàssica de veu: brillant, densa i sense greus que embrutin.", Plate,
              { { Mix, 22 }, { Decay, 2.0f }, { PreDelay, 30 }, { Size, 55 }, { Damping, 30 }, { LowCut, 200 }, { HighCut, 12000 }, { DecGreus, 0.6f }, { DecAguts, 0.85f }, { Eq2F, 3500 }, { Eq2G, -2 } } },
            { "Vocal Hall", CVeu, "Sala de concerts per a balades: cua suau i natural.", Hall,
              { { Mix, 20 }, { Decay, 2.6f }, { PreDelay, 35 }, { Size, 70 }, { Damping, 45 }, { LowCut, 180 }, { HighCut, 11000 }, { ModDepth, 25 } } },
            { "Vintage Vocal", CVeu, "Reverb digital dels 80 amb cor ample: veus pop amb sabor d'època.", Digital,
              { { Mix, 22 }, { Decay, 2.2f }, { PreDelay, 20 }, { Size, 55 }, { Damping, 50 }, { ModMode, 2 }, { ModDepth, 45 }, { HighCut, 9000 }, { LowCut, 160 } } },
            { "Wide Backing Vocals", CVeu, "Cambra ampla per als cors: els obre als costats i deixa el centre per a la veu.", Chamber,
              { { Mix, 28 }, { Decay, 1.8f }, { PreDelay, 15 }, { Size, 55 }, { Width, 150 }, { Separacio, 95 }, { EntradaMode, 1 }, { LowCut, 250 }, { Damping, 45 } } },
            { "Vocal Slap Ambience", CVeu, "Reflexió curta tipus «slap» dels 50: presència i caràcter sense cua.", Early,
              { { Mix, 18 }, { PreDelay, 75 }, { Size, 70 }, { Diffusion, 30 }, { Damping, 50 }, { LowCut, 250 }, { HighCut, 8000 }, { Width, 70 } } },
            { "Dark Vocal", CVeu, "Sala fosca: la cua no competeix amb les sibilants de la veu.", Hall,
              { { Mix, 20 }, { Decay, 2.4f }, { PreDelay, 30 }, { Size, 65 }, { Damping, 75 }, { HighCut, 6000 }, { DecAguts, 0.4f }, { EqHighG, -4 }, { LowCut, 180 } } },
            { "Epic Vocal", CVeu, "Sala enorme amb ducking per a tornades grans: la veu queda clara i la cua omple.", Hall,
              { { Mix, 26 }, { Decay, 4.5f }, { PreDelay, 60 }, { Size, 90 }, { Width, 130 }, { ModDepth, 40 }, { DuckOn, 1 }, { DuckThr, -30 }, { DuckAmt, 8 }, { DuckRel, 450 }, { LowCut, 200 } } },
            { "Vocal Delay Reverb", CVeu, "Pre-delay a tempo (1/8) i poca difusió: un eco que es fon en reverb.", Hall,
              { { Mix, 22 }, { Decay, 2.8f }, { PreSync, 1 }, { PreNota, 6 }, { Diffusion, 35 }, { EarlyLate, 75 }, { Size, 70 }, { DuckOn, 1 }, { DuckAmt, 6 }, { LowCut, 220 } } },
            { "Modern Pop Vocal", CVeu, "Placa brillant a tempo, amb ducking i greus nets: pop actual.", Plate,
              { { Mix, 20 }, { Decay, 1.6f }, { PreSync, 1 }, { PreNota, 3 }, { LowCut, 250 }, { HighCut, 13000 }, { EqLowG, -3 }, { DuckOn, 1 }, { DuckThr, -30 }, { DuckAmt, 7 }, { Width, 120 }, { Damping, 25 } } },
            { "Dry Vocal Space", CVeu, "Només reflexions molt curtes: la veu sembla seca però no plana.", Early,
              { { Mix, 12 }, { PreDelay, 4 }, { Size, 25 }, { Diffusion, 70 }, { Damping, 50 }, { LowCut, 200 } } },
            { "Dreamy Vocal", CVeu, "Ambient llarg i ondulant, amb ducking: veus de somni.", Ambient,
              { { Mix, 30 }, { Decay, 5.5f }, { PreDelay, 40 }, { ModMode, 1 }, { ModDepth, 60 }, { Damping, 35 }, { DuckOn, 1 }, { DuckAmt, 10 }, { LowCut, 220 }, { Width, 130 } } },
            { "Cathedral Vocal", CVeu, "Catedral de pedra: cua molt llarga i majestuosa.", Hall,
              { { Mix, 25 }, { Decay, 7.5f }, { PreDelay, 50 }, { Size, 100 }, { Damping, 50 }, { DecGreus, 1.2f }, { Width, 130 }, { LowCut, 150 }, { ModDepth, 25 } } },

            // ---------------- GUITARRES (15) ----------------
            { "Tight Guitar Room", CGui, "Habitació petita i seca per a guitarres rítmiques: les posa a l'espai sense allunyar-les.", Room,
              { { Mix, 16 }, { Decay, 0.6f }, { PreDelay, 5 }, { Size, 35 }, { Damping, 45 }, { EarlyLate, 40 }, { LowCut, 150 } } },
            { "Vintage Spring Guitar", CGui, "El moll d'un amplificador clàssic: vibrant i amb «boing».", Spring,
              { { Mix, 25 }, { Decay, 2.2f }, { Size, 50 }, { Diffusion, 70 }, { ModDepth, 30 }, { LowCut, 120 }, { HighCut, 7000 }, { Width, 90 } } },
            { "Wide Electric Guitar", CGui, "Cambra ampla per obrir una guitarra elèctrica mono a tot l'estèreo.", Chamber,
              { { Mix, 22 }, { Decay, 1.5f }, { PreDelay, 12 }, { Size, 55 }, { Width, 150 }, { Separacio, 95 }, { EntradaMode, 1 }, { LowCut, 180 } } },
            { "Clean Guitar Ambience", CGui, "Ambient suau per a arpegis nets: brillantor i aire.", Ambient,
              { { Mix, 24 }, { Decay, 3.0f }, { PreDelay, 25 }, { Damping, 35 }, { ModDepth, 40 }, { LowCut, 200 }, { Width, 120 } } },
            { "Indie Guitar Hall", CGui, "Sala gran amb modulació: guitarres indie amples i obertes.", Hall,
              { { Mix, 26 }, { Decay, 3.4f }, { PreDelay, 30 }, { Size, 80 }, { ModDepth, 45 }, { ModMode, 1 }, { LowCut, 200 }, { Width, 125 }, { DuckOn, 1 }, { DuckAmt, 5 } } },
            { "Acoustic Studio", CGui, "Estudi de fusta per a guitarra acústica: natural i proper.", Room,
              { { Mix, 18 }, { Decay, 1.1f }, { PreDelay, 12 }, { Size, 50 }, { Damping, 40 }, { DecGreus, 0.8f }, { LowCut, 120 }, { EarlyLate, 50 } } },
            { "Shimmer Guitar", CGui, "Cua que puja una octava: guitarres celestials.", Shimmer,
              { { Mix, 30 }, { Decay, 6.0f }, { PreDelay, 30 }, { ShimPitch, 0 }, { ShimAmt, 40 }, { ModDepth, 45 }, { LowCut, 250 }, { Damping, 30 }, { DuckOn, 1 }, { DuckAmt, 6 } } },
            { "Big Guitar Wall", CGui, "Sala densa i ampla per a guitarres distorsionades: mur de so sense embrutar.", Chamber,
              { { Mix, 18 }, { Decay, 2.0f }, { PreDelay, 20 }, { Size, 75 }, { Width, 140 }, { LowCut, 300 }, { HighCut, 8000 }, { DecGreus, 0.5f }, { Damping, 55 } } },
            { "Vintage Plate Guitar", CGui, "Placa dels 60 per a solos i guitarres netes.", Plate,
              { { Mix, 22 }, { Decay, 2.4f }, { PreDelay, 15 }, { Size, 60 }, { Damping, 50 }, { HighCut, 9000 }, { LowCut, 180 }, { ModDepth, 15 } } },
            { "Dark Guitar Room", CGui, "Habitació fosca i greu: guitarres de rock amb cos.", Room,
              { { Mix, 18 }, { Decay, 1.0f }, { PreDelay, 8 }, { Size, 45 }, { Damping, 75 }, { HighCut, 5500 }, { DecAguts, 0.4f }, { LowCut, 120 } } },
            { "Surf Spring Drip", CGui, "Molls molt presents i llargs: el so surf de sempre.", Spring,
              { { Mix, 40 }, { Decay, 3.5f }, { Size, 70 }, { Diffusion, 85 }, { ModDepth, 40 }, { LowCut, 150 }, { HighCut, 8000 } } },
            { "Lead Guitar Halo", CGui, "Sala amb pre-delay a tempo i ducking: el solo queda davant i la cua l'envolta.", Hall,
              { { Mix, 24 }, { Decay, 2.8f }, { PreSync, 1 }, { PreNota, 6 }, { Size, 75 }, { DuckOn, 1 }, { DuckThr, -26 }, { DuckAmt, 9 }, { LowCut, 250 }, { ModDepth, 35 } } },
            { "Acoustic Fingerstyle Hall", CGui, "Sala mitjana i clara per a guitarra acústica fingerstyle.", Hall,
              { { Mix, 22 }, { Decay, 2.0f }, { PreDelay, 25 }, { Size, 55 }, { Damping, 35 }, { DecGreus, 0.8f }, { LowCut, 120 }, { HighCut, 13000 } } },
            { "Ambient Swell Guitar", CGui, "Reverb al revés: cada nota creix des del no-res.", Reverse,
              { { Mix, 35 }, { Decay, 1.2f }, { Diffusion, 70 }, { LowCut, 200 }, { HighCut, 9000 }, { Width, 120 } } },
            { "Crunch Chamber", CGui, "Cambra petita i densa per a guitarres crunch: les engreixa.", Chamber,
              { { Mix, 16 }, { Decay, 1.1f }, { PreDelay, 10 }, { Size, 40 }, { Damping, 50 }, { LowCut, 250 }, { HighCut, 9000 } } },

            // ---------------- BATERIA I PERCUSSIÓ (15) ----------------
            { "Tight Snare Room", CBat, "Habitació curta per a la caixa: cos i espai sense cua.", Room,
              { { Mix, 20 }, { Decay, 0.5f }, { PreDelay, 0 }, { Size, 30 }, { Damping, 40 }, { EarlyLate, 45 }, { LowCut, 180 } } },
            { "Snare Plate", CBat, "Placa brillant per a la caixa: el clàssic dels discos de rock.", Plate,
              { { Mix, 24 }, { Decay, 1.6f }, { PreDelay, 10 }, { Size, 50 }, { Damping, 25 }, { LowCut, 200 }, { DecGreus, 0.6f } } },
            { "Gated Snare 80s", CBat, "Reverb tallada dels 80: caixa enorme que es talla en sec.", Gated,
              { { Mix, 35 }, { Decay, 0.9f }, { PreDelay, 0 }, { Size, 60 }, { Diffusion, 95 }, { LowCut, 150 }, { Width, 130 } } },
            { "Big Arena Drums", CBat, "Estadi: bateries de rock gegants.", Hall,
              { { Mix, 22 }, { Decay, 3.0f }, { PreDelay, 25 }, { Size, 95 }, { Width, 140 }, { LowCut, 150 }, { DecGreus, 0.7f }, { Damping, 45 } } },
            { "Drum Room", CBat, "La sala de bateria d'un estudi: natural, per a tot el kit.", Room,
              { { Mix, 20 }, { Decay, 0.9f }, { PreDelay, 3 }, { Size, 55 }, { Damping, 45 }, { EarlyLate, 50 }, { LowCut, 100 } } },
            { "Vintage Drum Chamber", CBat, "Cambra de reverberació dels 60-70: bateria amb vellut.", Chamber,
              { { Mix, 22 }, { Decay, 1.5f }, { PreDelay, 8 }, { Size, 55 }, { Damping, 60 }, { HighCut, 9000 }, { LowCut, 150 } } },
            { "Short Kick Ambience", CBat, "Un toc d'espai al bombo sense emmascarar els greus.", Early,
              { { Mix, 12 }, { PreDelay, 0 }, { Size, 30 }, { Diffusion, 60 }, { LowCut, 120 }, { HighCut, 6000 }, { Width, 60 } } },
            { "Huge Toms", CBat, "Sala gran i fosca per a timbals: rodons i llargs.", Hall,
              { { Mix, 26 }, { Decay, 2.4f }, { PreDelay, 15 }, { Size, 85 }, { Damping, 60 }, { DecGreus, 1.1f }, { LowCut, 70 }, { Width, 130 } } },
            { "Percussion Hall", CBat, "Sala oberta per a percussions de mà: aire i amplada.", Hall,
              { { Mix, 24 }, { Decay, 1.8f }, { PreDelay, 20 }, { Size, 60 }, { Damping, 35 }, { Width, 140 }, { LowCut, 200 } } },
            { "Drum Bus Glue", CBat, "Ambient molt curt per al bus de bateria: ho cohesiona tot.", Room,
              { { Mix, 12 }, { Decay, 0.45f }, { PreDelay, 0 }, { Size, 40 }, { Diffusion, 85 }, { LowCut, 200 }, { Width, 110 } } },
            { "Overheads Air", CBat, "Aire i brillantor per als overheads i platerets.", Plate,
              { { Mix, 14 }, { Decay, 1.2f }, { PreDelay, 12 }, { Size, 40 }, { LowCut, 500 }, { Damping, 20 }, { EqHighG, 2 } } },
            { "Brush Jazz Room", CBat, "Club de jazz petit per a bateria amb escombretes.", Room,
              { { Mix, 20 }, { Decay, 1.0f }, { PreDelay, 10 }, { Size, 45 }, { Damping, 55 }, { HighCut, 10000 }, { LowCut, 100 } } },
            { "Reverse Snare Swell", CBat, "Caixa al revés que creix fins al cop.", Reverse,
              { { Mix, 40 }, { Decay, 0.7f }, { Diffusion, 80 }, { LowCut, 200 }, { Width, 120 } } },
            { "Hand Percussion Plate", CBat, "Placa curta i ampla per a shakers, panderetes i congues.", Plate,
              { { Mix, 18 }, { Decay, 1.0f }, { PreDelay, 18 }, { Size, 40 }, { Width, 140 }, { LowCut, 300 } } },
            { "Electronic Clap Space", CBat, "Espai sincronitzat per a claps i caixes electròniques.", Hall,
              { { Mix, 25 }, { Decay, 1.6f }, { PreSync, 1 }, { PreNota, 3 }, { Size, 60 }, { LowCut, 300 }, { Width, 140 }, { Damping, 30 } } },

            // ---------------- BAIX (5) ----------------
            { "Short Bass Room", CBaix, "Habitació molt curta: el baix agafa espai sense perdre pegada.", Room,
              { { Mix, 10 }, { Decay, 0.4f }, { PreDelay, 0 }, { Size, 30 }, { LowCut, 250 }, { Width, 70 } } },
            { "Warm Bass Ambience", CBaix, "Ambient càlid per a baix elèctric: cos i aire en els harmònics.", Chamber,
              { { Mix, 12 }, { Decay, 0.9f }, { PreDelay, 5 }, { Size, 40 }, { LowCut, 300 }, { Damping, 60 }, { HighCut, 7000 } } },
            { "Synth Bass Space", CBaix, "Placa curta per a baixos de sintetitzador: amplada als aguts.", Plate,
              { { Mix, 14 }, { Decay, 1.0f }, { PreDelay, 10 }, { LowCut, 400 }, { Width, 140 }, { Damping, 30 } } },
            { "Vintage Bass Chamber", CBaix, "Cambra dels 60 per a baix de Motown.", Chamber,
              { { Mix, 12 }, { Decay, 1.2f }, { PreDelay, 8 }, { Size, 45 }, { LowCut, 220 }, { HighCut, 6000 }, { Damping, 65 } } },
            { "Sub Controlled Reverb", CBaix, "Reverb amb els greus tallats i en mono: el sub queda net.", Hall,
              { { Mix, 14 }, { Decay, 1.6f }, { PreDelay, 15 }, { LowCut, 500 }, { DecGreus, 0.3f }, { GreusMono, 1 }, { DuckOn, 1 }, { DuckAmt, 8 } } },

            // ---------------- PIANO I TECLATS (10) ----------------
            { "Grand Piano Hall", CPiano, "Sala de concerts per a piano de cua: rica i clara.", Hall,
              { { Mix, 24 }, { Decay, 2.6f }, { PreDelay, 25 }, { Size, 75 }, { Damping, 40 }, { DecGreus, 1.1f }, { LowCut, 80 }, { Width, 120 } } },
            { "Intimate Piano", CPiano, "Habitació petita i suau: piano proper i íntim.", Room,
              { { Mix, 16 }, { Decay, 1.0f }, { PreDelay, 8 }, { Size, 40 }, { Damping, 55 }, { LowCut, 100 } } },
            { "Vintage Electric Piano", CPiano, "Reverb digital amb cor per a pianos elèctrics dels 80.", Digital,
              { { Mix, 22 }, { Decay, 1.8f }, { PreDelay, 15 }, { ModMode, 2 }, { ModDepth, 50 }, { LowCut, 150 }, { HighCut, 9000 } } },
            { "Piano Plate", CPiano, "Placa per a piano pop: brillant i que no s'enfanga.", Plate,
              { { Mix, 20 }, { Decay, 1.8f }, { PreDelay, 20 }, { Size, 55 }, { LowCut, 200 }, { DecGreus, 0.6f } } },
            { "Wide Keyboard", CPiano, "Cambra ampla per a teclats mono: els obre a tot l'estèreo.", Chamber,
              { { Mix, 22 }, { Decay, 1.6f }, { PreDelay, 12 }, { Width, 150 }, { Separacio, 90 }, { EntradaMode, 1 }, { LowCut, 150 } } },
            { "Soft Rhodes Room", CPiano, "Habitació de fusta per a Rhodes: càlida i rodona.", Room,
              { { Mix, 20 }, { Decay, 1.3f }, { PreDelay, 10 }, { Size, 50 }, { Damping, 60 }, { HighCut, 8000 }, { ModDepth, 30 } } },
            { "Organ Church", CPiano, "Església per a orgue: llarga i majestuosa.", Hall,
              { { Mix, 30 }, { Decay, 5.5f }, { PreDelay, 35 }, { Size, 100 }, { Damping, 50 }, { DecGreus, 1.3f }, { LowCut, 60 }, { Width, 130 } } },
            { "Upright Piano Room", CPiano, "La sala d'estar amb el piano de paret.", Room,
              { { Mix, 18 }, { Decay, 0.8f }, { PreDelay, 5 }, { Size, 45 }, { Damping, 50 }, { EarlyLate, 45 }, { LowCut, 90 } } },
            { "Clavinet Spring", CPiano, "Moll curt per a clavinet i teclats funk.", Spring,
              { { Mix, 18 }, { Decay, 1.4f }, { Size, 35 }, { Diffusion, 60 }, { LowCut, 200 }, { HighCut, 7000 } } },
            { "Felt Piano Ambience", CPiano, "Ambient tou per a piano amb sordina: textura i somni.", Ambient,
              { { Mix, 30 }, { Decay, 4.0f }, { PreDelay, 20 }, { Damping, 60 }, { ModDepth, 45 }, { ModMode, 1 }, { LowCut, 120 } } },

            // ---------------- SINTES I PADS (10) ----------------
            { "Wide Synth", CSinte, "Sala ampla per a sintes: obre i separa.", Hall,
              { { Mix, 25 }, { Decay, 2.2f }, { PreDelay, 20 }, { Width, 150 }, { Separacio, 95 }, { ModDepth, 40 }, { LowCut, 200 } } },
            { "Shimmer Pad", CSinte, "Shimmer d'octava per a pads: s'eleva cap amunt.", Shimmer,
              { { Mix, 40 }, { Decay, 8.0f }, { PreDelay, 20 }, { ShimPitch, 0 }, { ShimAmt, 50 }, { ModDepth, 50 }, { LowCut, 200 }, { Width, 130 } } },
            { "Infinite Pad", CSinte, "Cua gairebé infinita: el pad no s'acaba mai.", Ambient,
              { { Mix, 45 }, { Decay, 15.0f }, { Feedback, 85 }, { ModDepth, 55 }, { ModMode, 1 }, { LowCut, 200 }, { Damping, 45 } } },
            { "Ambient Keys", CSinte, "Ambient per a teclats suaus: espai i moviment.", Ambient,
              { { Mix, 30 }, { Decay, 4.5f }, { PreDelay, 25 }, { ModDepth, 45 }, { LowCut, 150 }, { Damping, 40 } } },
            { "Vintage Digital Synth", CSinte, "La reverb digital dels 80 per a sintes polifònics.", Digital,
              { { Mix, 28 }, { Decay, 3.0f }, { PreDelay, 20 }, { Size, 70 }, { ModMode, 2 }, { ModDepth, 55 }, { HighCut, 10000 } } },
            { "Dreamy Synth", CSinte, "Cua ondulant amb modulació aleatòria: tons de somni.", Ambient,
              { { Mix, 35 }, { Decay, 6.0f }, { PreDelay, 30 }, { ModMode, 1 }, { ModDepth, 70 }, { ModRate, 0.3f }, { Damping, 50 } } },
            { "Huge Synth Hall", CSinte, "Sala gegant per a leads i acords grans.", Hall,
              { { Mix, 30 }, { Decay, 5.0f }, { PreDelay, 40 }, { Size, 100 }, { Width, 140 }, { ModDepth, 40 }, { LowCut, 180 } } },
            { "Pluck Space", CSinte, "Placa curta per a sintes pluck: defineix sense omplir.", Plate,
              { { Mix, 22 }, { Decay, 1.2f }, { PreSync, 1 }, { PreNota, 3 }, { LowCut, 300 }, { Width, 130 } } },
            { "Arp Sync Verb", CSinte, "Pre-delay i modulació a tempo per a arpegiadors.", Hall,
              { { Mix, 26 }, { Decay, 2.0f }, { PreSync, 1 }, { PreNota, 5 }, { ModSync, 1 }, { ModNota, 12 }, { ModDepth, 40 }, { Diffusion, 55 }, { DuckOn, 1 }, { DuckAmt, 6 } } },
            { "Lead Synth Plate", CSinte, "Placa amb ducking per a leads: presents i amb cua entre frases.", Plate,
              { { Mix, 22 }, { Decay, 2.2f }, { PreDelay, 25 }, { DuckOn, 1 }, { DuckThr, -26 }, { DuckAmt, 10 }, { LowCut, 250 } } },

            // ---------------- ESPAIS ACÚSTICS (10) ----------------
            { "Small Studio", CEspai, "Estudi petit i tractat: el so de la sala de gravació.", Room,
              { { Mix, 20 }, { Decay, 0.5f }, { PreDelay, 3 }, { Size, 30 }, { Damping, 60 }, { EarlyLate, 45 } } },
            { "Live Room", CEspai, "Sala gran d'estudi amb parets vives.", Room,
              { { Mix, 22 }, { Decay, 1.2f }, { PreDelay, 8 }, { Size, 75 }, { Damping, 35 }, { EarlyLate, 50 } } },
            { "Recording Booth", CEspai, "Cabina de veu: gairebé sec, només una mica d'aire.", Early,
              { { Mix, 15 }, { PreDelay, 1 }, { Size, 12 }, { Diffusion, 80 }, { Damping, 70 } } },
            { "Medium Hall", CEspai, "Sala mitjana: el punt de partida per a gairebé tot.", Hall,
              { { Mix, 25 }, { Decay, 2.2f }, { PreDelay, 20 }, { Size, 60 }, { Damping, 40 } } },
            { "Large Concert Hall", CEspai, "Gran sala de concerts simfònica.", Hall,
              { { Mix, 28 }, { Decay, 3.4f }, { PreDelay, 35 }, { Size, 90 }, { Damping, 45 }, { DecGreus, 1.15f }, { Width, 120 } } },
            { "Cathedral", CEspai, "Catedral de pedra: més de 8 segons de cua.", Hall,
              { { Mix, 32 }, { Decay, 8.5f }, { PreDelay, 60 }, { Size, 100 }, { Damping, 50 }, { DecGreus, 1.3f }, { Width, 130 }, { ModDepth, 20 } } },
            { "Stone Chamber", CEspai, "Cambra de pedra: densa i brillant.", Chamber,
              { { Mix, 25 }, { Decay, 2.0f }, { PreDelay, 12 }, { Size, 60 }, { Damping, 20 }, { Diffusion, 90 } } },
            { "Wooden Room", CEspai, "Sala de fusta: càlida i suau.", Room,
              { { Mix, 22 }, { Decay, 1.0f }, { PreDelay, 6 }, { Size, 55 }, { Damping, 65 }, { DecGreus, 1.2f }, { HighCut, 9000 } } },
            { "Tiled Bathroom", CEspai, "Lavabo amb rajoles: petit, brillant i ressonant.", Chamber,
              { { Mix, 25 }, { Decay, 1.1f }, { PreDelay, 2 }, { Size, 20 }, { Damping, 10 }, { Diffusion, 55 }, { DecAguts, 1.0f } } },
            { "Scoring Stage", CEspai, "Plató d'enregistrament de bandes sonores: ample i equilibrat.", Hall,
              { { Mix, 26 }, { Decay, 2.4f }, { PreDelay, 28 }, { Size, 85 }, { Damping, 38 }, { Width, 125 }, { EarlyLate, 55 } } },

            // ---------------- AMBIENT I CINEMA (10) ----------------
            { "Infinite Atmosphere", CAmb, "Atmosfera sense final: tot el que hi entra hi queda.", Ambient,
              { { Mix, 50 }, { Decay, 20.0f }, { Feedback, 95 }, { ModDepth, 50 }, { ModMode, 1 }, { LowCut, 150 }, { Damping, 50 }, { Width, 140 } } },
            { "Dark Space", CAmb, "Espai fosc i profund: tensió per a escenes de cinema.", Ambient,
              { { Mix, 40 }, { Decay, 9.0f }, { PreDelay, 40 }, { Damping, 80 }, { HighCut, 5000 }, { DecAguts, 0.3f }, { ModDepth, 40 } } },
            { "Frozen Texture", CAmb, "Textura quieta i estable. Prem Freeze per congelar-la del tot.", Ambient,
              { { Mix, 50 }, { Decay, 14.0f }, { Feedback, 90 }, { ModDepth, 25 }, { FreezeRel, 5 }, { Damping, 40 }, { Width, 130 } } },
            { "Cinematic Hall", CAmb, "Sala enorme per a orquestra i tràilers.", Hall,
              { { Mix, 32 }, { Decay, 5.5f }, { PreDelay, 45 }, { Size, 100 }, { Width, 140 }, { DecGreus, 1.2f }, { ModDepth, 30 } } },
            { "Ethereal Shimmer", CAmb, "Shimmer de quinta: brillant i misteriós.", Shimmer,
              { { Mix, 42 }, { Decay, 9.0f }, { PreDelay, 30 }, { ShimPitch, 1 }, { ShimAmt, 45 }, { ModDepth, 55 }, { LowCut, 250 } } },
            { "Deep Ambient", CAmb, "Ambient llarg i greu, molt difús.", Ambient,
              { { Mix, 45 }, { Decay, 12.0f }, { PreDelay, 20 }, { Damping, 60 }, { DecGreus, 1.4f }, { ModDepth, 45 }, { LowCut, 60 } } },
            { "Floating Clouds", CAmb, "Núvols de so amb shimmer suau i modulació lenta.", Shimmer,
              { { Mix, 45 }, { Decay, 10.0f }, { ShimPitch, 0 }, { ShimAmt, 30 }, { ModMode, 1 }, { ModDepth, 65 }, { ModRate, 0.2f }, { Damping, 45 }, { Width, 145 } } },
            { "Distant Horizon", CAmb, "So llunyà: pre-delay llarg i cua que s'allunya.", Hall,
              { { Mix, 40 }, { Decay, 6.5f }, { PreDelay, 180 }, { Size, 100 }, { Damping, 65 }, { EarlyLate, 90 }, { HighCut, 8000 } } },
            { "Sub Octave Drone", CAmb, "Shimmer cap avall (-12): drons foscos i greus.", Shimmer,
              { { Mix, 45 }, { Decay, 9.0f }, { ShimPitch, 3 }, { ShimAmt, 40 }, { Damping, 55 }, { LowCut, 60 }, { ModDepth, 35 } } },
            { "Slow Bloom", CAmb, "Ambient que floreix lentament: entra suau i s'obre.", Ambient,
              { { Mix, 38 }, { Decay, 7.0f }, { PreDelay, 120 }, { Diffusion, 100 }, { ModDepth, 50 }, { EarlyLate, 100 }, { Damping, 40 } } },

            // ---------------- CREATIUS I FX (10) ----------------
            { "Reverse Impact", CFx, "Swell al revés per a impactes i transicions.", Reverse,
              { { Mix, 50 }, { Decay, 2.0f }, { Diffusion, 85 }, { Width, 140 }, { LowCut, 120 } } },
            { "Infinite Freeze", CFx, "Cua infinita: toca una nota i prem Freeze per quedar-te-la.", Ambient,
              { { Mix, 55 }, { Decay, 20.0f }, { Feedback, 100 }, { ModDepth, 30 }, { FreezeRel, 8 }, { Width, 140 } } },
            { "Pitch Up Reverb", CFx, "Cada volta de la cua puja una octava.", Shimmer,
              { { Mix, 45 }, { Decay, 6.0f }, { ShimPitch, 0 }, { ShimAmt, 70 }, { Damping, 30 }, { LowCut, 300 } } },
            { "Pitch Down Reverb", CFx, "Cada volta de la cua baixa una octava.", Shimmer,
              { { Mix, 45 }, { Decay, 6.0f }, { ShimPitch, 3 }, { ShimAmt, 65 }, { Damping, 40 }, { LowCut, 80 } } },
            { "Metallic Space", CFx, "Espai metàl·lic i ressonant: poca difusió, molt brillant.", Plate,
              { { Mix, 35 }, { Decay, 3.5f }, { Size, 20 }, { Diffusion, 5 }, { Damping, 0 }, { DecAguts, 1.6f }, { ModDepth, 0 } } },
            { "Huge Experimental", CFx, "Shimmer de quarta, feedback i modulació salvatge.", Shimmer,
              { { Mix, 55 }, { Decay, 15.0f }, { ShimPitch, 2 }, { ShimAmt, 55 }, { Feedback, 60 }, { ModMode, 1 }, { ModDepth, 90 }, { ModRate, 2.5f }, { Width, 150 } } },
            { "Rhythmic Reverb", CFx, "Gate a tempo: la cua bombeja amb la música.", Hall,
              { { Mix, 40 }, { Decay, 3.0f }, { PreSync, 1 }, { PreNota, 6 }, { GateOn, 1 }, { GateThr, -30 }, { GateHold, 120 }, { GateRel, 60 }, { Width, 140 } } },
            { "Long Modulated Tail", CFx, "Cua molt llarga i plena de moviment.", Digital,
              { { Mix, 40 }, { Decay, 12.0f }, { ModMode, 2 }, { ModDepth, 85 }, { ModRate, 1.2f }, { Damping, 40 }, { Width, 140 } } },
            { "Reverse Tail", CFx, "Cua normal però girada al revés: efecte de cinta al revés.", Hall,
              { { Mix, 45 }, { Decay, 2.0f }, { RevOn, 1 }, { Size, 70 }, { LowCut, 150 }, { Width, 130 } } },
            { "Ducked Pump Verb", CFx, "Ducking fort: la reverb bombeja entre els cops.", Plate,
              { { Mix, 40 }, { Decay, 3.0f }, { DuckOn, 1 }, { DuckThr, -40 }, { DuckAtk, 3 }, { DuckRel, 180 }, { DuckAmt, 24 }, { Width, 140 } } },
        };

        std::vector<Preset> creaFabrica()
        {
            std::vector<Preset> v;
            for (const auto& d : defs)
            {
                Preset p;
                p.nom = d.nom;
                p.categoria = cats[(size_t) d.cat];
                p.descripcio = d.desc;
                p.p.posa (Alg, (float) d.alg);
                for (const auto& [k, x] : d.v) p.p.posa (k, x);
                v.push_back (std::move (p));
            }
            return v;
        }

        // Treu accents i majúscules (UTF-8 de les lletres catalanes i castellanes).
        std::string normalitza (const std::string& s)
        {
            static const std::pair<const char*, char> mapa[] = {
                { "à", 'a' }, { "á", 'a' }, { "è", 'e' }, { "é", 'e' }, { "í", 'i' }, { "ï", 'i' }, { "ò", 'o' }, { "ó", 'o' }, { "ú", 'u' }, { "ü", 'u' },
                { "ç", 'c' }, { "ñ", 'n' }, { "À", 'a' }, { "È", 'e' }, { "É", 'e' }, { "Í", 'i' }, { "Ò", 'o' }, { "Ó", 'o' }, { "Ú", 'u' }, { "Ç", 'c' } };
            std::string r;
            for (size_t i = 0; i < s.size();)
            {
                bool fet = false;
                for (const auto& [u, c] : mapa)
                {
                    const size_t n = std::char_traits<char>::length (u);
                    if (s.compare (i, n, u) == 0) { r += c; i += n; fet = true; break; }
                }
                if (! fet) { r += (char) std::tolower ((unsigned char) s[i]); ++i; }
            }
            return r;
        }
    }

    const std::vector<std::string>& categories() { return cats; }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = creaFabrica();
        return v;
    }

    const Preset& presetInicial()
    {
        for (const auto& p : presetsFabrica()) if (p.nom == "Medium Hall") return p;
        return presetsFabrica().front();
    }

    bool coincideix (const std::string& text, const std::string& cerca)
    {
        const auto c = normalitza (cerca);
        if (c.empty()) return true;
        const auto t = normalitza (text);
        // Totes les paraules de la cerca hi han de ser.
        size_t a = 0;
        while (a < c.size())
        {
            size_t b = c.find (' ', a);
            if (b == std::string::npos) b = c.size();
            if (b > a && t.find (c.substr (a, b - a)) == std::string::npos) return false;
            a = b + 1;
        }
        return true;
    }

    // ---- Biblioteca de l'usuari ----
    bool BibliotecaPresets::carrega (const std::filesystem::path& fitxer)
    {
        usuari.clear();
        favorits.clear();
        std::error_code ec;
        if (! std::filesystem::exists (fitxer, ec)) return true;   // encara no n'hi ha: biblioteca buida
        try
        {
            std::ifstream f (fitxer, std::ios::binary);
            const auto j = nlohmann::json::parse (f);
            for (const auto& x : j.value ("presets", nlohmann::json::array()))
            {
                Preset p;
                p.nom = x.value ("nom", std::string());
                p.categoria = x.value ("carpeta", std::string());
                p.descripcio = x.value ("descripcio", std::string());
                if (p.nom.empty() || ! p.p.llegeix (x.value ("estat", std::string()))) continue;
                if (busca (p.nom) == nullptr) usuari.push_back (std::move (p));
            }
            for (const auto& x : j.value ("favorits", nlohmann::json::array()))
                if (x.is_string()) favorits.push_back (x.get<std::string>());
            return true;
        }
        catch (...) { return false; }
    }

    bool BibliotecaPresets::desa (const std::filesystem::path& fitxer) const
    {
        nlohmann::json j;
        j["versio"] = 1;
        j["presets"] = nlohmann::json::array();
        for (const auto& p : usuari)
            j["presets"].push_back ({ { "nom", p.nom }, { "carpeta", p.categoria }, { "descripcio", p.descripcio }, { "estat", p.p.text() } });
        j["favorits"] = favorits;
        std::error_code ec;
        std::filesystem::create_directories (fitxer.parent_path(), ec);
        auto tmp = fitxer;
        tmp += ".tmp";
        {
            std::ofstream f (tmp, std::ios::binary | std::ios::trunc);
            if (! f) return false;
            f << j.dump (1);
            if (! f) return false;
        }
        std::filesystem::rename (tmp, fitxer, ec);
        if (ec)
        {
            std::filesystem::remove (fitxer, ec);
            std::filesystem::rename (tmp, fitxer, ec);
        }
        return ! ec;
    }

    void BibliotecaPresets::desaPreset (const Preset& p)
    {
        for (auto& x : usuari)
            if (x.nom == p.nom) { x = p; return; }
        usuari.push_back (p);
    }

    bool BibliotecaPresets::esborra (const std::string& nom)
    {
        const auto it = std::find_if (usuari.begin(), usuari.end(), [&] (const Preset& p) { return p.nom == nom; });
        if (it == usuari.end()) return false;
        usuari.erase (it);
        favorits.erase (std::remove (favorits.begin(), favorits.end(), clauUsuari (nom)), favorits.end());
        return true;
    }

    bool BibliotecaPresets::reanomena (const std::string& nom, const std::string& nou)
    {
        if (nou.empty() || nou == nom || busca (nou) != nullptr) return false;
        for (auto& p : usuari)
            if (p.nom == nom)
            {
                p.nom = nou;
                for (auto& f : favorits) if (f == clauUsuari (nom)) f = clauUsuari (nou);
                return true;
            }
        return false;
    }

    bool BibliotecaPresets::mou (const std::string& nom, const std::string& carpeta)
    {
        for (auto& p : usuari)
            if (p.nom == nom) { p.categoria = carpeta; return true; }
        return false;
    }

    const Preset* BibliotecaPresets::busca (const std::string& nom) const
    {
        for (const auto& p : usuari) if (p.nom == nom) return &p;
        return nullptr;
    }

    std::vector<std::string> BibliotecaPresets::carpetes() const
    {
        std::vector<std::string> v;
        for (const auto& p : usuari)
            if (std::find (v.begin(), v.end(), p.categoria) == v.end()) v.push_back (p.categoria);
        std::sort (v.begin(), v.end());
        return v;
    }

    bool BibliotecaPresets::esFavorit (const std::string& clau) const
    {
        return std::find (favorits.begin(), favorits.end(), clau) != favorits.end();
    }

    void BibliotecaPresets::commutaFavorit (const std::string& clau)
    {
        const auto it = std::find (favorits.begin(), favorits.end(), clau);
        if (it != favorits.end()) favorits.erase (it);
        else favorits.push_back (clau);
    }

    std::string BibliotecaPresets::nomLliure (const std::string& base) const
    {
        if (busca (base) == nullptr) return base;
        for (int i = 2;; ++i)
        {
            auto n = base + " " + std::to_string (i);
            if (busca (n) == nullptr) return n;
        }
    }
}
