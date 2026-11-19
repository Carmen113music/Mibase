#include "mibase/Equalitzador.h"
#include <algorithm>
#include <cmath>

/** 0.63: presets de fàbrica de l'EQ Pro. Punts de partida musicals: cada cançó i cada micròfon és diferent, però
    serveixen per no començar de zero (i per aprendre on és cada cosa). */
namespace mibase::eq
{
    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c { "Veus", "Guitarres", "Baix", "Bateria", "Teclats i sintes", "Busos i mescla",
                                                  "Mastering", "Correccions", "Creatius" };
        return c;
    }

    namespace
    {
        Banda bd (int t, float f, float g, float q, int pend = 1, int canal = Estereo)
        {
            Banda b; b.usada = true; b.tipus = t; b.freq = f; b.guany = g; b.q = q; b.pendent = pend; b.canal = canal;
            return b;
        }
        Banda HP (float f, int pend = 3) { return bd (HighPass, f, 0, qDefecte, pend); }
        Banda LP (float f, int pend = 1) { return bd (LowPass, f, 0, qDefecte, pend); }
        Banda BL (float f, float g, float q = 1.0f) { return bd (Bell, f, g, q); }
        Banda LS (float f, float g, float q = qDefecte) { return bd (LowShelf, f, g, q); }
        Banda HS (float f, float g, float q = qDefecte) { return bd (HighShelf, f, g, q); }
        Banda NT (float f, float q = 8.0f) { return bd (Notch, f, 0, q); }
        Banda TL (float f, float g) { return bd (Tilt, f, g, qDefecte); }
        Banda BP (float f, float q = 1.0f) { return bd (BandPass, f, 0, q); }
        Banda C (Banda b, int canal) { b.canal = canal; return b; }
        Banda SC (Banda b) { b.sidechain = true; return b; }   // el detector escolta el sidechain
        /** Banda dinàmica: llindar, ràtio, rang (+ atac i retorn). */
        Banda D (Banda b, float llindar, float ratio, float rang, float atac = 5.0f, float retorn = 80.0f)
        {
            b.dinamica = true; b.llindar = llindar; b.ratio = ratio; b.rang = rang; b.atac = atac; b.retorn = retorn;
            return b;
        }

        struct Def { const char* cat; const char* nom; const char* desc; float out; std::vector<Banda> b; };

        const std::vector<Def>& taula()
        {
            static const std::vector<Def> t {
                // ---- Veus ----
                { "Veus", "Veu pop neta", "Treu greus del micròfon, una mica de fang i dóna aire a dalt.", 0, { HP (90), BL (300, -2.5f, 1.2f), BL (3200, 1.5f, 0.9f), HS (11000, 3.0f) } },
                { "Veus", "Veu pop moderna", "Brillant i endavant, amb de-esser dinàmic per a les «s».", 0, { HP (100), BL (250, -3, 1.0f), BL (4000, 2.0f, 0.8f), D (BL (7000, 0, 3.0f), -30, 4, -6), HS (12000, 4.0f) } },
                { "Veus", "Veu íntima", "Propera i càlida: cos al mig-greu i aguts suaus.", 0, { HP (70, 2), LS (180, 1.5f), BL (2500, -1.0f, 1.0f), HS (9000, 1.5f) } },
                { "Veus", "Veu rock", "Presència per passar entre guitarres.", 0, { HP (120), BL (400, -2.0f, 1.2f), BL (2000, 3.0f, 1.0f), HS (8000, 2.0f) } },
                { "Veus", "Veu indie", "Mig marcat i aguts continguts, una mica vintage.", 0, { HP (110), BL (800, 1.5f, 0.9f), BL (2800, 2.0f, 1.2f), LP (14000) } },
                { "Veus", "Veu de balada", "Cos i calidesa sense enfangar; aire molt suau.", 0, { HP (80, 2), BL (200, 1.0f, 0.8f), BL (500, -1.5f, 1.4f), HS (10000, 2.0f) } },
                { "Veus", "Veu parlada", "Locució clara: fora greus, intel·ligibilitat al mig.", 0, { HP (100), BL (180, 1.0f, 1.0f), BL (350, -2.0f, 1.5f), BL (3500, 2.0f, 1.0f) } },
                { "Veus", "Rap", "Fort al mig-agut perquè cada síl·laba es noti.", 0, { HP (100), BL (300, -2.0f, 1.0f), BL (3000, 3.0f, 1.0f), D (BL (6500, 0, 3.0f), -28, 4, -5) } },
                { "Veus", "Cors i doblatges", "Més prims que la veu principal per no tapar-la.", -1, { HP (180), BL (400, -3.0f, 1.0f), BL (2500, -1.5f, 1.0f), HS (10000, 3.0f) } },
                { "Veus", "Veu ambient", "Lleugera i aèria, per a capes i dream pop.", 0, { HP (200, 4), BL (600, -2.0f, 0.8f), HS (8000, 5.0f) } },
                { "Veus", "Veu ronca (de-mud)", "Treu el «boom» del micròfon a prop (efecte de proximitat).", 0, { HP (90), D (BL (200, 0, 1.2f), -28, 3, -6, 10, 120), BL (450, -2.0f, 1.5f) } },
                { "Veus", "Veu nasal", "Rebaixa la zona nasal (1 kHz) de manera suau.", 0, { HP (80), BL (1000, -3.0f, 2.0f), HS (10000, 1.5f) } },
                { "Veus", "Veu estrident", "Domestica els 2,5–4 kHz quan la veu crida.", 0, { HP (90), D (BL (3000, 0, 2.0f), -26, 4, -6, 3, 60), HS (11000, 1.5f) } },
                { "Veus", "De-esser suau", "Només baixa les «s» i «x» quan apareixen.", 0, { D (BL (6500, 0, 3.0f), -32, 4, -5, 1, 60) } },
                { "Veus", "De-esser fort", "Per a sibilants molt marcades (o aguts pujats).", 0, { D (HS (5500, 0), -34, 6, -10, 1, 60) } },
                { "Veus", "Veu de telèfon", "Banda estreta: efecte de ràdio vella o telèfon.", 3, { HP (400, 4), LP (3200, 4), BL (1500, 4.0f, 1.0f) } },
                { "Veus", "Veu de megàfon", "Mig agressiu i res als extrems.", 2, { HP (600, 5), LP (5000, 4), BL (2200, 6.0f, 1.5f) } },
                { "Veus", "Veu de ràdio FM", "Greus rodons i aguts brillants, com un locutor.", 0, { HP (60), LS (150, 3.0f), BL (400, -2.0f, 1.2f), HS (9000, 3.0f) } },
                { "Veus", "Xiuxiueig", "Treu el soroll de baix i realça l'aire.", 0, { HP (150, 4), BL (700, -2.0f, 1.0f), HS (6000, 4.0f) } },
                { "Veus", "Adlibs", "Prims i brillants, al costat de la principal.", -2, { HP (250, 4), BL (1500, 2.0f, 1.0f), HS (9000, 3.0f) } },
                // ---- Guitarres ----
                { "Guitarres", "Acústica rasguejada", "Menys boom del cos, més definició de les cordes.", 0, { HP (90), BL (220, -3.0f, 1.0f), BL (2500, 1.5f, 1.0f), HS (10000, 2.5f) } },
                { "Guitarres", "Acústica dits", "Cos natural i aguts suaus per a punteigs.", 0, { HP (70, 2), BL (180, 1.0f, 0.9f), BL (900, -1.5f, 1.2f), HS (8000, 2.0f) } },
                { "Guitarres", "Acústica en mescla", "Prima per deixar lloc al baix i la veu.", 0, { HP (150, 4), BL (350, -2.5f, 1.0f), HS (9000, 3.0f) } },
                { "Guitarres", "Acústica ressonant", "Controla la ressonància del cos (100–200 Hz) només quan bufa.", 0, { HP (80), D (BL (140, 0, 2.0f), -28, 3, -6, 10, 150), HS (10000, 2.0f) } },
                { "Guitarres", "Elèctrica neta", "Brillantor de vidre per a arpegis nets.", 0, { HP (90), BL (300, -1.5f, 1.0f), BL (3000, 2.0f, 0.9f), HS (7000, 2.0f) } },
                { "Guitarres", "Elèctrica crunch", "Presència i menys fang per al crunch.", 0, { HP (100), BL (500, -2.0f, 1.2f), BL (1800, 2.0f, 1.0f), LP (12000) } },
                { "Guitarres", "Distorsió rítmica", "Mur de guitarres que no tapa el baix.", 0, { HP (110), BL (250, -2.0f, 1.0f), BL (700, -1.5f, 2.0f), BL (2500, 1.5f, 1.0f), LP (10000, 2) } },
                { "Guitarres", "Distorsió amb pes", "Més cos per a riffs greus.", 0, { HP (80), LS (150, 2.0f), BL (800, -2.0f, 1.5f), LP (9000, 2) } },
                { "Guitarres", "Solo endavant", "Mig-agut empès perquè el solo es senti.", 1, { HP (150), BL (1200, 2.5f, 1.0f), BL (3500, 2.0f, 1.2f), LP (11000) } },
                { "Guitarres", "Fizz fora", "Treu el «brunzit» àspre de les distorsions digitals.", 0, { HP (100), D (BL (5000, 0, 2.0f), -34, 3, -6), LP (8500, 3) } },
                { "Guitarres", "Indie jangle", "Brillant i aeri, molt de «chime».", 0, { HP (120), BL (400, -2.0f, 1.0f), HS (5000, 3.5f) } },
                { "Guitarres", "Guitarra vintage", "Talla a dalt i a baix: so de radiocasset.", 0, { HP (200, 4), BL (1200, 2.0f, 0.8f), LP (6000, 3) } },
                { "Guitarres", "Amplificador fosc", "Aguts suaus i mig rodó.", 0, { HP (80), BL (600, 1.5f, 0.8f), HS (4000, -4.0f) } },
                { "Guitarres", "Doble guitarra (estèreo)", "Les obre: menys mig al centre, més aire als costats.", 0, { HP (100), C (BL (600, -2.0f, 1.0f), Mid), C (HS (6000, 3.0f), Side) } },
                // ---- Baix ----
                { "Baix", "Baix rodó", "Fonamental ferma i menys fang.", 0, { HP (35, 3), LS (80, 2.0f), BL (250, -2.5f, 1.2f), LP (8000) } },
                { "Baix", "Baix amb atac", "Que se senti la pua o el dit al costat del bombo.", 0, { HP (35, 3), BL (300, -2.0f, 1.0f), BL (1000, 2.0f, 1.0f), BL (2500, 2.5f, 1.2f) } },
                { "Baix", "Baix pop", "Rodó a baix i definit a 700 Hz per a altaveus petits.", 0, { HP (40, 3), BL (90, 2.0f, 1.0f), BL (250, -2.0f, 1.2f), BL (700, 2.0f, 1.0f) } },
                { "Baix", "Baix rock", "Mig agressiu i greus continguts.", 0, { HP (45, 3), BL (120, 1.5f, 1.0f), BL (500, -1.5f, 1.5f), BL (1200, 3.0f, 1.0f) } },
                { "Baix", "Baix dub", "Molt de greu i res a dalt.", 0, { HP (30, 3), LS (100, 5.0f), LP (1500, 3) } },
                { "Baix", "Baix i bombo (sidechain)", "Baixa els greus del baix quan sona el bombo (posa el bombo al SC).", 0, { HP (35, 3), SC (D (LS (120, 0), -30, 4, -6, 1, 100)), BL (800, 1.5f, 1.0f) } },
                { "Baix", "Baix sintètic", "Sub controlat i un punt de brillantor.", 0, { HP (28, 3), D (LS (60, 0), -18, 3, -4, 5, 120), BL (300, -2.0f, 1.0f), HS (3000, 2.0f) } },
                { "Baix", "Baix de contrabaix", "Treu el boom de la caixa i dóna articulació.", 0, { HP (40, 3), BL (150, -2.0f, 1.0f), BL (1500, 2.0f, 1.0f) } },
                { "Baix", "Notes que ressonen", "Baixa les notes greus que sobresurten quan sonen.", 0, { HP (35, 3), D (BL (100, 0, 3.0f), -24, 4, -6, 10, 150), D (BL (160, 0, 3.0f), -24, 4, -6, 10, 150) } },
                { "Baix", "Baix prim (mescla densa)", "Deixa lloc al bombo i als sintes greus.", 0, { HP (70, 4), BL (250, -2.0f, 1.0f), BL (900, 2.0f, 1.0f) } },
                // ---- Bateria ----
                { "Bateria", "Bombo modern", "Cop a 60 Hz, menys cartró i el clic de la maça.", 0, { HP (30, 3), BL (60, 3.0f, 1.2f), BL (350, -4.0f, 1.5f), BL (3500, 3.0f, 1.2f) } },
                { "Bateria", "Bombo rock", "Rodó i amb punta, menys ressonant.", 0, { HP (35, 3), BL (80, 2.5f, 1.0f), BL (400, -3.5f, 2.0f), BL (2500, 2.5f, 1.0f) } },
                { "Bateria", "Bombo vintage", "Suau, sense clic: setanta.", 0, { HP (40, 2), BL (100, 2.0f, 0.8f), BL (400, -2.0f, 1.0f), LP (6000, 2) } },
                { "Bateria", "Caixa amb cos", "Cos a 200 Hz i cop a 5 kHz; fora la «ring».", 0, { HP (80, 3), BL (200, 2.5f, 1.2f), NT (900, 10.0f), HS (5000, 3.0f) } },
                { "Bateria", "Caixa seca", "Més pal i menys bordó.", 0, { HP (120, 3), BL (500, -2.0f, 1.5f), BL (2000, 2.0f, 1.2f) } },
                { "Bateria", "Caixa ressonant (dinàmica)", "Baixa la ressonància només quan la caixa sona.", 0, { HP (90, 3), D (BL (800, 0, 4.0f), -30, 4, -6, 1, 100), HS (6000, 2.0f) } },
                { "Bateria", "Charles", "Fora greus i brillantor neta, sense estridència.", 0, { HP (300, 4), D (BL (3500, 0, 2.0f), -28, 3, -4), HS (10000, 2.0f) } },
                { "Bateria", "Overheads", "Plats oberts i menys tambors al greu.", 0, { HP (150, 3), BL (400, -2.0f, 1.0f), HS (10000, 3.0f) } },
                { "Bateria", "Overheads foscos", "Plats suaus per a cançons tranquil·les.", 0, { HP (100, 3), HS (6000, -3.0f), LP (14000) } },
                { "Bateria", "Toms", "Cop greu i atac, fora la caixa del mig.", 0, { HP (60, 3), BL (100, 2.0f, 1.0f), BL (500, -3.0f, 1.5f), BL (4000, 2.0f, 1.0f) } },
                { "Bateria", "Room", "Ambient de sala: més cos i menys plats.", 0, { HP (60, 3), LS (150, 2.0f), HS (6000, -3.0f) } },
                { "Bateria", "Bus de bateria", "Pes i brillantor per a tot el kit.", 0, { HP (30, 3), LS (80, 2.0f), BL (400, -1.5f, 1.0f), HS (10000, 2.0f) } },
                { "Bateria", "Bateria lo-fi", "Aplanada i fosca, de cinta vella.", 1, { HP (100, 3), BL (1000, 3.0f, 0.7f), LP (5000, 3) } },
                { "Bateria", "Caixes de ritmes", "Sub ferm i aguts d'electrònica.", 0, { HP (25, 3), LS (60, 3.0f), BL (300, -2.0f, 1.0f), HS (8000, 3.0f) } },
                { "Bateria", "Percussió", "Ni greus ni fang: la brillantor dels pals.", 0, { HP (200, 3), BL (600, -2.0f, 1.0f), HS (7000, 3.0f) } },
                { "Bateria", "Pandereta i shaker", "Aïlla la brillantor, molt per sota del charles.", -2, { HP (500, 4), HS (8000, 2.0f) } },
                // ---- Teclats i sintes ----
                { "Teclats i sintes", "Piano pop", "Brillant i definit, sense fang.", 0, { HP (60, 2), BL (250, -2.0f, 1.0f), BL (3000, 1.5f, 1.0f), HS (10000, 2.0f) } },
                { "Teclats i sintes", "Piano càlid", "Cos al mig-greu i aguts suaus.", 0, { HP (40, 2), LS (150, 1.5f), HS (8000, -2.0f) } },
                { "Teclats i sintes", "Piano en mescla", "Prim perquè no xoqui amb la guitarra i la veu.", 0, { HP (150, 3), BL (400, -2.0f, 1.0f), BL (2500, -1.5f, 1.0f), HS (9000, 2.0f) } },
                { "Teclats i sintes", "Rhodes", "Rodó i càlid, amb un punt de campana.", 0, { HP (60, 2), BL (300, 1.5f, 0.8f), BL (1800, 1.5f, 1.2f), LP (9000) } },
                { "Teclats i sintes", "Orgue", "Treu les bombes de greus i la duresa.", 0, { HP (70, 3), BL (2500, -2.0f, 1.0f), LP (10000) } },
                { "Teclats i sintes", "Pad que no tapa", "Fora greus i mig: el pad envolta sense molestar.", 0, { HP (200, 3), BL (500, -3.0f, 0.8f), HS (8000, 2.0f) } },
                { "Teclats i sintes", "Pad ample", "Més costats i menys centre.", 0, { HP (150, 3), C (BL (800, -3.0f, 0.8f), Mid), C (HS (5000, 3.0f), Side) } },
                { "Teclats i sintes", "Lead de sinte", "Presència per sobre de tot.", 0, { HP (180, 3), BL (1500, 2.0f, 1.0f), HS (6000, 2.0f) } },
                { "Teclats i sintes", "Arpegiador", "Brillant i prim per a arpegis ràpids.", 0, { HP (250, 3), BL (3000, 2.0f, 1.0f), HS (9000, 2.0f) } },
                { "Teclats i sintes", "Corda sintètica", "Seda a dalt i menys caixa.", 0, { HP (120, 2), BL (600, -2.0f, 1.0f), HS (9000, 3.0f) } },
                { "Teclats i sintes", "Sub 808", "Greu net: fora el que hi ha per sota de 30 Hz i el fang.", 0, { HP (28, 4), BL (55, 2.0f, 1.2f), BL (300, -3.0f, 1.0f) } },
                { "Teclats i sintes", "Pluck", "Atac definit, cua neta.", 0, { HP (200, 3), BL (2000, 2.0f, 1.0f), BL (500, -2.0f, 1.0f) } },
                // ---- Busos i mescla ----
                { "Busos i mescla", "Bus de veus", "Uneix les veus amb aire i menys fang.", 0, { HP (100), BL (300, -1.5f, 0.8f), HS (10000, 2.0f) } },
                { "Busos i mescla", "Bus de guitarres", "Lloc per al baix i la veu.", 0, { HP (100), BL (350, -1.5f, 1.0f), BL (3000, -1.0f, 1.0f) } },
                { "Busos i mescla", "Retorn de reverb", "Reverb neta: sense greus que enfanguen ni aguts que xiulen.", 0, { HP (300, 3), LP (8000, 2), BL (600, -2.0f, 1.0f) } },
                { "Busos i mescla", "Retorn de delay", "Repeticions que no xoquen amb la veu.", 0, { HP (400, 3), LP (5000, 2) } },
                { "Busos i mescla", "Mescla més clara", "Menys fang a tota la cançó.", 0, { BL (300, -1.5f, 0.7f), HS (12000, 1.0f) } },
                { "Busos i mescla", "Mescla més càlida", "Una mica més de greu i aguts més suaus.", 0, { LS (120, 1.5f), HS (8000, -1.0f) } },
                { "Busos i mescla", "Tilt brillant", "Inclina tot cap als aguts (més brillant).", 0, { TL (1000, 2.0f) } },
                { "Busos i mescla", "Tilt fosc", "Inclina tot cap als greus (més fosc).", 0, { TL (1000, -2.0f) } },
                { "Busos i mescla", "Greus al centre", "El greu, en mono; els costats, sense greus.", 0, { C (HP (120, 3), Side) } },
                { "Busos i mescla", "Mono per sota de 80", "Elimina greus dels costats (més ferm i compatible).", 0, { C (HP (80, 4), Side) } },
                // ---- Mastering ----
                { "Mastering", "Màster neutre", "Neteja de subgreus i un toc d'aire.", 0, { HP (25, 3), HS (14000, 1.0f) } },
                { "Mastering", "Màster pop", "Brillant i amb pes, a punt per al limitador.", 0, { HP (25, 3), LS (60, 1.0f), BL (300, -1.0f, 0.7f), HS (12000, 1.5f) } },
                { "Mastering", "Màster rock", "Mig present i greu ferm.", 0, { HP (28, 3), LS (80, 1.0f), BL (2500, 1.0f, 0.7f), HS (12000, 1.0f) } },
                { "Mastering", "Màster indie", "Càlid, amb mig rodó i aguts continguts.", 0, { HP (25, 3), BL (200, 0.8f, 0.7f), BL (3500, -0.8f, 1.0f), HS (14000, 0.8f) } },
                { "Mastering", "Màster acústic", "Natural: gairebé res, només ordre.", 0, { HP (22, 3), BL (250, -0.8f, 0.8f), HS (12000, 0.8f) } },
                { "Mastering", "Màster electrònic", "Sub ferm i aguts que brillen.", 0, { HP (22, 4), LS (50, 1.5f), HS (10000, 2.0f) } },
                { "Mastering", "Màster càlid", "Arrodoneix una mescla massa dura.", 0, { HP (25, 3), LS (100, 1.0f), BL (3000, -1.0f, 0.8f), HS (10000, -0.5f) } },
                { "Mastering", "Màster ample", "Una mica més de costats a dalt; greus en mono.", 0, { HP (25, 3), C (HP (100, 3), Side), C (HS (8000, 1.5f), Side) } },
                { "Mastering", "Màster dinàmic (domador)", "Controla greus i duresa només quan sobresurten.", 0, { HP (25, 3), D (LS (100, 0), -16, 2, -2, 20, 200), D (BL (3000, 0, 1.0f), -22, 2, -2, 10, 150) } },
                { "Mastering", "Màster de-esser", "Baixa les sibilants de tota la mescla.", 0, { HP (25, 3), D (BL (7000, 0, 2.0f), -28, 3, -3, 1, 60) } },
                // ---- Correccions ----
                { "Correccions", "Brunzit de 50 Hz", "Treu la xarxa elèctrica d'Europa (50, 100, 150 Hz).", 0, { NT (50, 12.0f), NT (100, 12.0f), NT (150, 12.0f) } },
                { "Correccions", "Brunzit de 60 Hz", "La xarxa d'Amèrica (60, 120, 180 Hz).", 0, { NT (60, 12.0f), NT (120, 12.0f), NT (180, 12.0f) } },
                { "Correccions", "Fora subgreus", "Tall net per sota de 30 Hz.", 0, { HP (30, 5) } },
                { "Correccions", "Fora fang", "Treu la zona enfangada (200–400 Hz).", 0, { BL (300, -3.0f, 1.0f) } },
                { "Correccions", "Fora duresa", "Suavitza els 2–4 kHz.", 0, { BL (3000, -3.0f, 1.0f) } },
                { "Correccions", "Fora soroll de cinta", "Aguts suaus per a enregistraments amb soroll.", 0, { HS (8000, -4.0f), LP (14000) } },
                { "Correccions", "Fora soroll de vent", "Talla els cops de vent i de taula.", 0, { HP (120, 5) } },
                { "Correccions", "Recerca de ressonàncies", "Una banda estreta i pujada: mou-la escoltant-la i baixa-la on molesti.", 0, { BL (1000, 10.0f, 8.0f) } },
                // ---- Creatius ----
                { "Creatius", "Ràdio antiga", "Banda estreta, mig nasal.", 4, { HP (500, 5), LP (3500, 5), BL (1500, 4.0f, 1.2f) } },
                { "Creatius", "A través de la paret", "Com sentir la música des de l'habitació del costat.", 0, { LP (400, 4), LS (100, 2.0f) } },
                { "Creatius", "Altaveu de mòbil", "Res de greus: com sona en un telèfon.", 2, { HP (700, 5), BL (2500, 3.0f, 1.0f), LP (9000, 3) } },
                { "Creatius", "Lo-fi", "Fosc i estret, de cinta.", 1, { HP (150, 3), LP (5000, 3), BL (1000, 2.0f, 0.7f) } },
                { "Creatius", "Filtre de pista (intro)", "Passa-baixos: automatitza la freqüència per obrir el so.", 0, { LP (600, 3) } },
                { "Creatius", "Filtre agut (pujada)", "Passa-alts: automatitza'l per a les pujades abans de la tornada.", 0, { HP (500, 3) } },
                { "Creatius", "Telèfon amb distorsió", "Mig molt pujat (posa-hi una saturació després).", 0, { HP (800, 5), LP (3000, 5), BL (1700, 8.0f, 2.0f) } },
                { "Creatius", "Només els costats", "Fora el centre del greu i mig (el so s'obre).", 0, { C (LS (500, -12.0f), Mid) } },
                { "Creatius", "Campana", "Banda passant estreta: un so que sona com dins d'un tub.", 0, { BP (1200, 4.0f) } },
                { "Creatius", "Molt d'aire", "Aguts exagerats per a capes etèries.", -2, { HP (300, 3), HS (7000, 8.0f) } },
            };
            return t;
        }
    }

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& d : taula())
            {
                Equalitzador e;
                for (size_t k = 0; k < d.b.size() && k < (size_t) maxBandes; ++k) e.posaBanda ((int) k, d.b[k]);
                e.posaSortida (d.out);
                r.push_back ({ d.nom, d.cat, d.desc, e.estat() });
            }
            return r;
        }();
        return v;
    }

    double guanyAuto (const std::vector<Banda>& bandes, double fs)
    {
        // potència mitjana per octava (soroll rosa), de 20 Hz a 20 kHz, amb la corba de cada canal (L i R)
        const int punts = 240;
        double suma = 0.0;
        for (int i = 0; i < punts; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (i + 0.5) / punts);
            double pot = 0.0;
            for (int canal : { 0, 1 })
            {
                std::complex<double> h (1.0, 0.0);
                for (const auto& b : bandes)
                {
                    if (! b.usada || ! b.activa) continue;
                    // L i R reben: Estèreo, el seu canal, i la meitat de Mid i Side (aproximació per a la potència)
                    const bool toca = b.canal == Estereo || (b.canal == Esquerre && canal == 0) || (b.canal == Dret && canal == 1);
                    const bool mig = b.canal == Mid || b.canal == Side;
                    if (! toca && ! mig) continue;
                    Seccio s[maxSeccions];
                    const int ns = dissenya (b, fs, s);
                    auto r = resposta (s, ns, f, fs);
                    if (mig) r = 0.5 * (r + std::complex<double> (1.0, 0.0));
                    h *= r;
                }
                pot += std::norm (h);
            }
            suma += pot * 0.5;
        }
        const double mitjana = suma / punts;
        return mitjana > 1.0e-12 ? std::clamp (-10.0 * std::log10 (mitjana), -24.0, 24.0) : 0.0;
    }
}
