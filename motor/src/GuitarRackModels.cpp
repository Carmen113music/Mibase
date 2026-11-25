#include "mibase/GuitarRack.h"

#include <algorithm>
#include <cctype>

// 1.5: el catàleg del Guitar Rack: pedals, amplis, pantalles, altaveus i micròfons.
// Els noms són de Mibase; «inspirat» diu de quin equip real ve la idea. Els sons són DSP propi (sense mostres ni IR).
namespace mibase::guitarrack
{
    const char* nomCategoria (int c)
    {
        static const char* const n[numCategories] = { "Drive", "Compressors", "Modulació", "Delay", "Reverb", "Wah i filtres", "Altres" };
        return c >= 0 && c < numCategories ? n[c] : "";
    }

    namespace
    {
        Control pom (const char* clau, const char* nom, float def = 5.0f, const char* ajuda = "", float mn = 0.0f, float mx = 10.0f)
        {
            Control c; c.clau = clau; c.nom = nom; c.def = def; c.mn = mn; c.mx = mx; c.ajuda = ajuda; return c;
        }
        Control ms (const char* clau, const char* nom, float mn, float mx, float def, const char* ajuda = "")
        {
            Control c = pom (clau, nom, def, ajuda, mn, mx); c.unitat = "ms"; c.log = true; return c;
        }
        Control db (const char* clau, const char* nom, float mn, float mx, float def, const char* ajuda = "")
        {
            Control c = pom (clau, nom, def, ajuda, mn, mx); c.unitat = "dB"; return c;
        }
        Control sel (const char* clau, const char* nom, std::vector<std::string> op, int def = 0, const char* ajuda = "")
        {
            Control c = pom (clau, nom, (float) def, ajuda, 0.0f, (float) op.size() - 1.0f);
            c.tipus = op.size() == 2 ? TipusControl::Interruptor : TipusControl::Selector;
            c.opcions = std::move (op);
            return c;
        }
        Control ped (const char* clau = "pedal", const char* nom = "Pedal", float def = 50.0f)
        {
            Control c = pom (clau, nom, def, "La posició del pedal: arrossega amunt i avall (també es pot automatitzar).", 0.0f, 100.0f);
            c.tipus = TipusControl::Pedal; c.unitat = "%"; return c;
        }

        ModelPedal pd (const char* clau, const char* nom, const char* inspirat, const char* marca, int cat, int motor, int variant,
                       std::uint32_t color, std::uint32_t text, Forma f, const char* caracter, const char* estils, const char* desc,
                       std::vector<Control> c, std::uint32_t colorPom = 0xff161616)
        {
            ModelPedal p;
            p.clau = clau; p.nom = nom; p.inspirat = inspirat; p.marca = marca; p.categoria = cat; p.motor = motor; p.variant = variant;
            p.color = color; p.colorText = text; p.forma = f; p.caracter = caracter; p.estils = estils; p.descripcio = desc;
            p.controls = std::move (c); p.colorPom = colorPom;
            return p;
        }

        std::vector<ModelPedal> creaPedals()
        {
            using F = Forma;
            const auto B = 0xff0e0e0e, W = 0xfff4f4f4;
            std::vector<ModelPedal> v;
            // ---------------------------------------------------------------- overdrive i distorsió
            v.push_back (pd ("ts808", "Screamer 808", "Ibanez Tube Screamer TS808", "Ibanez", CatDrive, MDrive, 0, 0xff3d9a5c, W, F::Petit,
                             "overdrive suau, mitjos, comprimit", "blues rock indie solo", "El clàssic overdrive de mitjos: ajusta els greus i empeny l'ampli sense embrutar-lo.",
                             { pom ("drive", "Drive", 3), pom ("tone", "Tone", 5.5f), pom ("level", "Level", 6) }));
            v.push_back (pd ("ts9", "Screamer 9", "Ibanez Tube Screamer TS9", "Ibanez", CatDrive, MDrive, 1, 0xff4fae68, W, F::Petit,
                             "overdrive mitjos, una mica més brillant", "rock blues", "Germà del 808 amb una sortida més brillant i un punt més dur.",
                             { pom ("drive", "Drive", 4), pom ("tone", "Tone", 6), pom ("level", "Level", 6) }));
            v.push_back (pd ("sd1", "Super Drive SD", "Boss SD-1 Super OverDrive", "Boss", CatDrive, MDrive, 2, 0xffe9c529, B, F::Compacte,
                             "overdrive asimètric, mitjos", "rock indie metall", "Overdrive asimètric de mitjos: un clàssic per empènyer amplis de guany alt.",
                             { pom ("level", "Level", 6), pom ("tone", "Tone", 5), pom ("drive", "Drive", 4) }));
            v.push_back (pd ("ds1", "Dist One", "Boss DS-1 Distortion", "Boss", CatDrive, MDrive, 3, 0xffef7d1d, B, F::Compacte,
                             "distorsió dura, aguts", "punk grunge rock", "Distorsió de transistor i díodes: dura, aspra i amb molt de caràcter.",
                             { pom ("level", "Level", 5), pom ("tone", "Tone", 5), pom ("dist", "Dist", 6) }));
            v.push_back (pd ("rat", "Rodent", "ProCo RAT", "ProCo", CatDrive, MDrive, 4, 0xff1b1b1b, W, F::Gran,
                             "distorsió, fuzz, gruixut", "grunge indie alternatiu punk", "Distorsió d'operacional amb el famós Filter (més = més fosc).",
                             { pom ("dist", "Distortion", 6), pom ("filter", "Filter", 4), pom ("volume", "Volume", 5) }));
            v.push_back (pd ("bd2", "Blues Drive BD", "Boss BD-2 Blues Driver", "Boss", CatDrive, MDrive, 5, 0xff2f6fc4, W, F::Compacte,
                             "overdrive transparent, dinàmic, ampli", "blues indie rock clàssic", "Sona com un ampli que s'obre: respon molt a com toques.",
                             { pom ("level", "Level", 5), pom ("tone", "Tone", 5), pom ("gain", "Gain", 4) }));
            v.push_back (pd ("klon", "Centaur Gold", "Klon Centaur", "Klon", CatDrive, MDrive, 6, 0xffcaa84a, B, F::Petit,
                             "transparent, boost, net barrejat", "rock clàssic indie blues", "Barreja el so net amb el saturat: empeny sense perdre la guitarra.",
                             { pom ("gain", "Gain", 3), pom ("treble", "Treble", 5), pom ("output", "Output", 6) }));
            v.push_back (pd ("ocd", "Obsessive Drive", "Fulltone OCD", "Fulltone", CatDrive, MDrive, 7, 0xffe9e5d9, B, F::Petit,
                             "overdrive a distorsió, ampli britànic", "rock indie alternatiu", "De crunch a distorsió d'ampli britànic. HP és més obert i brillant.",
                             { pom ("volume", "Volume", 5), pom ("drive", "Drive", 5), pom ("tone", "Tone", 5), sel ("mode", "Mode", { "LP", "HP" }, 1) }));
            v.push_back (pd ("muff", "Big Fuzz Pi", "Electro-Harmonix Big Muff Pi", "Electro-Harmonix", CatDrive, MDrive, 8, 0xffc9ccd2, 0xffc8202a, F::Gran,
                             "fuzz, sustain llarg, mitjos buidats", "shoegaze grunge stoner psicodèlia", "Mur de fuzz amb sustain infinit i els mitjos buidats.",
                             { pom ("volume", "Volume", 5), pom ("tone", "Tone", 5), pom ("sustain", "Sustain", 7) }));
            v.push_back (pd ("distplus", "Distortion Plus", "MXR Distortion+", "MXR", CatDrive, MDrive, 9, 0xfff1d23e, B, F::Petit,
                             "distorsió de germani, vintage", "rock clàssic punk", "Distorsió senzilla i crua de díodes de germani.",
                             { pom ("output", "Output", 5), pom ("dist", "Distortion", 6) }));
            v.push_back (pd ("mt2", "Metal Zone MZ", "Boss Metal Zone MT-2", "Boss", CatDrive, MDrive, 10, 0xff353535, 0xffff6a2a, F::Compacte,
                             "distorsió molt alta, metall, EQ", "metall hardcore", "Molt de guany amb un EQ de tres bandes i mitjos escombrables.",
                             { pom ("level", "Level", 5), pom ("dist", "Dist", 7), db ("high", "High", -15, 15, 0), db ("low", "Low", -15, 15, 0),
                               db ("middle", "Middle", -15, 15, 0), pom ("midfreq", "Mid Freq", 5) }));
            v.push_back (pd ("beod", "Brown Eye OD", "Friedman BE-OD", "Friedman", CatDrive, MDrive, 11, 0xff202020, 0xffd8b46a, F::Petit,
                             "guany alt, ampli britànic modificat", "hard rock metall modern", "Un ampli britànic modificat en un pedal: guany alt, ajustat i amb presència.",
                             { pom ("volume", "Volume", 5), pom ("gain", "Gain", 6), pom ("bass", "Bass", 5), pom ("treble", "Treble", 5), pom ("tight", "Tight", 4), pom ("presence", "Presence", 5) }));
            // ---------------------------------------------------------------- compressors
            v.push_back (pd ("dynacomp", "Dyna Squeeze", "MXR Dyna Comp", "MXR", CatComp, MComp, 0, 0xffc8212b, W, F::Petit,
                             "compressió forta, sustain, country", "country funk indie net", "Compressor «squish»: igualat i amb molt de sustain.",
                             { pom ("output", "Output", 5), pom ("sens", "Sensitivity", 5) }));
            v.push_back (pd ("cs3", "Comp Sustainer", "Boss CS-3 Compression Sustainer", "Boss", CatComp, MComp, 1, 0xff62b8e2, B, F::Compacte,
                             "compressor, sustain, to", "funk pop net", "Compressor de sempre amb atac i to.",
                             { pom ("level", "Level", 5), pom ("tone", "Tone", 5), pom ("attack", "Attack", 5), pom ("sustain", "Sustain", 5) }));
            v.push_back (pd ("keeley", "Studio Comp", "Keeley Compressor Plus", "Keeley", CatComp, MComp, 2, 0xff6c7f92, W, F::Petit,
                             "compressió transparent, barreja", "indie pop net country", "Compressor transparent amb barreja en paral·lel (Blend).",
                             { pom ("sustain", "Sustain", 5), pom ("level", "Level", 5), pom ("blend", "Blend", 6), pom ("tone", "Tone", 5) }));
            v.push_back (pd ("cali76", "Studio 76", "Origin Effects Cali76", "Origin Effects", CatComp, MComp, 3, 0xffd9d2bf, 0xff1d3557, F::Petit,
                             "compressor d'estudi FET, ràpid", "estudi indie pop", "Compressor FET d'estudi: ràpid, amb ratio i barreja del so sec.",
                             { pom ("input", "Input", 5), pom ("output", "Output", 5), pom ("attack", "Attack", 5), pom ("release", "Release", 5),
                               sel ("ratio", "Ratio", { "4:1", "8:1", "12:1", "20:1" }, 0), pom ("dry", "Dry", 2) }));
            // ---------------------------------------------------------------- modulació
            v.push_back (pd ("ce2", "Chorus C2", "Boss CE-2 Chorus", "Boss", CatMod, MChorus, 0, 0xff80c9ea, B, F::Compacte,
                             "chorus càlid, analògic", "80s indie dream pop net", "Chorus analògic càlid i rodó.",
                             { pom ("rate", "Rate", 3), pom ("depth", "Depth", 5) }));
            v.push_back (pd ("smallclone", "Little Clone", "Electro-Harmonix Small Clone", "Electro-Harmonix", CatMod, MChorus, 1, 0xffbcc1c7, 0xff2050a0, F::Petit,
                             "chorus profund, líquid", "grunge indie shoegaze", "Chorus profund i aquós. L'interruptor el fa més intens.",
                             { pom ("rate", "Rate", 4), sel ("depth", "Depth", { "Normal", "Profund" }, 0) }));
            v.push_back (pd ("phase90", "Phase Ninety", "MXR Phase 90", "MXR", CatMod, MPhaser, 0, 0xffef6e1f, B, F::Petit,
                             "phaser de quatre etapes", "rock clàssic funk indie", "Phaser d'un sol pom: lent per a acords, ràpid per a solos.",
                             { pom ("speed", "Speed", 3) }));
            v.push_back (pd ("smallstone", "Little Stone", "Electro-Harmonix Small Stone", "Electro-Harmonix", CatMod, MPhaser, 1, 0xffb9bec4, 0xff1b1b1b, F::Petit,
                             "phaser, color", "funk psicodèlia indie", "Phaser amb l'interruptor Color: més ressonant.",
                             { pom ("rate", "Rate", 3), sel ("color", "Color", { "Off", "On" }, 0) }));
            v.push_back (pd ("bf2", "Flanger F2", "Boss BF-2 Flanger", "Boss", CatMod, MFlanger, 0, 0xff7b53a8, W, F::Compacte,
                             "flanger, avió", "80s rock psicodèlia", "Flanger clàssic: del xiuxiueig a l'avió.",
                             { pom ("manual", "Manual", 5), pom ("depth", "Depth", 5), pom ("rate", "Rate", 3), pom ("res", "Res", 5) }));
            v.push_back (pd ("univibe", "Vibe Machine", "Uni-Vibe", "Shin-ei / Dunlop", CatMod, MPhaser, 2, 0xff4d2d6a, 0xfff0d38a, F::Gran,
                             "vibe, fase pulsant, psicodèlic", "psicodèlia blues rock clàssic", "El vaivé del rotatiu fet amb llums i fotocèl·lules.",
                             { pom ("speed", "Speed", 4), pom ("intensity", "Intensity", 6), sel ("mode", "Mode", { "Chorus", "Vibrato" }, 0), pom ("volume", "Volume", 5) }));
            v.push_back (pd ("tr2", "Tremolo T2", "Boss TR-2 Tremolo", "Boss", CatMod, MTremolo, 0, 0xff2fa48b, W, F::Compacte,
                             "trèmolo, surf, vintage", "surf indie rockabilly", "Trèmolo de forma variable: de suau a tallat.",
                             { pom ("rate", "Rate", 5), pom ("wave", "Wave", 3), pom ("depth", "Depth", 6) }));
            // ---------------------------------------------------------------- delay
            v.push_back (pd ("dd3", "Digital Delay D3", "Boss DD-3 Digital Delay", "Boss", CatDelay, MDelay, 0, 0xfff1f1f1, B, F::Compacte,
                             "delay digital net", "rock pop indie", "Delay digital clar, de slapback a ecos llargs.",
                             { pom ("level", "E.Level", 4), pom ("feedback", "F.Back", 4), ms ("time", "D.Time", 20, 800, 380) }));
            v.push_back (pd ("dd7", "Digital Delay D7", "Boss DD-7 Digital Delay", "Boss", CatDelay, MDelay, 1, 0xffececec, B, F::Compacte,
                             "delay digital, analògic, modulat", "rock pop ambient", "Delay amb modes Standard, Analog i Modulate.",
                             { pom ("level", "E.Level", 4), pom ("feedback", "F.Back", 4), ms ("time", "D.Time", 20, 2000, 450),
                               sel ("mode", "Mode", { "Standard", "Analog", "Modulate" }, 0) }));
            v.push_back (pd ("memoryman", "Memory Lad", "Electro-Harmonix Memory Man", "Electro-Harmonix", CatDelay, MDelay, 2, 0xffc9cdd2, 0xffc8202a, F::Gran,
                             "delay analògic fosc, chorus", "indie post-rock shoegaze", "Delay analògic fosc i càlid amb chorus o vibrato als ecos.",
                             { pom ("level", "Level", 5), pom ("blend", "Blend", 4), pom ("feedback", "Feedback", 4), ms ("delay", "Delay", 30, 550, 380),
                               sel ("mod", "Mod", { "Chorus", "Vibrato" }, 0), pom ("depth", "Depth", 3) }));
            v.push_back (pd ("carbon", "Carbon Echo", "MXR Carbon Copy", "MXR", CatDelay, MDelay, 3, 0xff2e6b3e, W, F::Petit,
                             "delay analògic, fosc", "indie rock blues", "Delay analògic fosc, amb modulació opcional.",
                             { pom ("regen", "Regen", 4), pom ("mix", "Mix", 4), ms ("delay", "Delay", 20, 600, 350), sel ("mod", "Mod", { "Off", "On" }, 0) }));
            v.push_back (pd ("timeline", "Time Lines", "Strymon Timeline", "Strymon", CatDelay, MDelay, 4, 0xff2f60aa, W, F::Ample,
                             "delay d'estudi, cinta, ping-pong, sync", "ambient post-rock indie modern", "Delay d'estudi amb diversos tipus i sincronia amb el tempo.",
                             { sel ("type", "Type", { "Digital", "Cinta", "Analògic", "Ping-pong", "Lo-fi" }, 0), ms ("time", "Time", 20, 2000, 480),
                               pom ("repeats", "Repeats", 4), pom ("mix", "Mix", 4), pom ("filter", "Filter", 6), pom ("grit", "Grit", 1), pom ("mod", "Mod", 2),
                               sel ("tempo", "Tempo", { "Lliure", "1/4", "1/8·", "1/8", "1/4T", "1/16" }, 0) }));
            v.push_back (pd ("spaceecho", "Tape Echo 201", "Roland Space Echo RE-201", "Roland", CatDelay, MCinta, 0, 0xff2e3a32, 0xffe8e2cf, F::Ample,
                             "eco de cinta, wow, flutter, molla", "dub reggae psicodèlia indie", "Eco de cinta amb tres capçals, saturació i reverb de molla.",
                             { pom ("rate", "Repeat Rate", 5), pom ("intensity", "Intensity", 4), pom ("echo", "Echo Vol", 5),
                               sel ("mode", "Mode", { "1", "2", "3", "4", "5", "6", "7" }, 3), pom ("bass", "Bass", 5), pom ("treble", "Treble", 5), pom ("reverb", "Reverb", 2) }));
            // ---------------------------------------------------------------- reverb
            v.push_back (pd ("rv6", "Reverb R6", "Boss RV-6 Reverb", "Boss", CatReverb, MReverb, 0, 0xff2b4c7d, W, F::Compacte,
                             "reverb versàtil, shimmer", "pop indie ambient", "Reverb de tots els colors: sala, hall, plate, molla, modulada, shimmer…",
                             { pom ("level", "E.Level", 4), pom ("tone", "Tone", 5), pom ("time", "Time", 5),
                               sel ("mode", "Mode", { "Room", "Hall", "Plate", "Spring", "Modulate", "Shimmer", "Dynamic" }, 1) }));
            v.push_back (pd ("holygrail", "Holy Glow", "Electro-Harmonix Holy Grail", "Electro-Harmonix", CatReverb, MReverb, 1, 0xffc1c5ca, 0xff1a1a1a, F::Petit,
                             "reverb senzilla, molla, hall", "indie rock surf", "Un sol pom: molla, hall o «flerb» (reverb amb flanger).",
                             { pom ("reverb", "Reverb", 4), sel ("mode", "Mode", { "Spring", "Hall", "Flerb" }, 0) }));
            v.push_back (pd ("bigsky", "Wide Sky", "Strymon BigSky", "Strymon", CatReverb, MReverb, 2, 0xff3c4049, W, F::Ample,
                             "reverb d'estudi, ambient, núvols", "ambient post-rock shoegaze cinematogràfic", "Reverbs grans: sala, hall, plate, molla, bloom, núvol i shimmer.",
                             { sel ("type", "Type", { "Room", "Hall", "Plate", "Spring", "Bloom", "Cloud", "Shimmer" }, 1), pom ("decay", "Decay", 6),
                               ms ("predelay", "Pre-delay", 0, 300, 30), pom ("mix", "Mix", 4), pom ("tone", "Tone", 5), pom ("mod", "Mod", 3) }));
            v.push_back (pd ("fenderspring", "Spring Tank 63", "Fender Spring Reverb (6G15)", "Fender", CatReverb, MReverb, 3, 0xff4a3628, 0xffe8dcc0, F::Ample,
                             "molla, surf, goteig", "surf rockabilly indie", "La caixa de molla: Dwell perquè degoti, Mix per a la quantitat.",
                             { pom ("dwell", "Dwell", 6), pom ("mix", "Mix", 4), pom ("tone", "Tone", 5) }, 0xfff0ead8));
            v.push_back (pd ("plate", "Studio Plate", "Plate Reverb (EMT 140)", "Estudi", CatReverb, MReverb, 4, 0xff7b8088, W, F::Gran,
                             "plate d'estudi, suau", "pop rock indie", "Placa metàl·lica d'estudi: densa i brillant.",
                             { pom ("decay", "Decay", 5), ms ("predelay", "Pre-delay", 0, 200, 20), pom ("mix", "Mix", 3), pom ("damping", "Damping", 4) }));
            // ---------------------------------------------------------------- wah i filtres
            v.push_back (pd ("crybaby", "Cry Wah", "Dunlop Cry Baby", "Dunlop", CatWah, MWah, 0, 0xff1d1d1d, W, F::Basculant,
                             "wah clàssic", "funk rock clàssic blues", "Wah de sempre: mou el pedal (o automatitza'l).",
                             { ped() }));
            v.push_back (pd ("v847", "Vintage Wah 847", "Vox V847", "Vox", CatWah, MWah, 1, 0xff2b2b2b, 0xffd9d9d9, F::Basculant,
                             "wah vocal, suau", "rock clàssic psicodèlia", "Wah més vocal i suau, amb el rang una mica més greu.",
                             { ped() }));
            v.push_back (pd ("autowah", "Auto Wah", "Auto Wah (envolupant o LFO)", "Genèric", CatWah, MWah, 2, 0xff6b3d8c, W, F::Compacte,
                             "wah automàtic", "funk disco", "El wah es mou sol: amb la força de la pua o amb un LFO.",
                             { pom ("sens", "Sens", 5), pom ("range", "Range", 5), pom ("reso", "Reso", 5), sel ("mode", "Mode", { "Envolupant", "LFO" }, 0), pom ("rate", "Rate", 4) }));
            v.push_back (pd ("envfilter", "Envelope Filter", "Envelope Filter (Mu-Tron / Q-Tron)", "Genèric", CatWah, MWah, 3, 0xff9b5b2b, W, F::Petit,
                             "filtre d'envolupant, funky", "funk disco jam", "Filtre que s'obre amb la pua: funky i quacker.",
                             { pom ("sens", "Sens", 5), pom ("peak", "Peak", 6), sel ("range", "Range", { "Greus", "Aguts" }, 1),
                               sel ("mode", "Mode", { "LP", "BP", "HP" }, 1), sel ("dir", "Direcció", { "Amunt", "Avall" }, 0) }));
            // ---------------------------------------------------------------- altres
            v.push_back (pd ("oc2", "Octave O2", "Boss OC-2 Octave", "Boss", CatAltres, MPitch, 0, 0xff2d5ea0, W, F::Compacte,
                             "octava baixa analògica", "rock funk indie", "Una i dues octaves per sota, analògic (per a notes soles).",
                             { pom ("oct1", "Oct 1", 6), pom ("oct2", "Oct 2", 0), pom ("direct", "Direct", 7) }));
            v.push_back (pd ("pog", "Poly Octave", "Electro-Harmonix POG", "Electro-Harmonix", CatAltres, MPitch, 1, 0xffc0c4ca, 0xff1a3d7a, F::Gran,
                             "octavador polifònic, orgue", "ambient indie post-rock", "Octaves per sota i per sobre, també amb acords: sona a orgue.",
                             { pom ("dry", "Dry", 8), pom ("sub", "Sub Oct", 0), pom ("up", "Oct Up", 5), pom ("up2", "2 Oct Up", 0) }));
            v.push_back (pd ("whammy", "Pitch Whammer", "DigiTech Whammy", "DigiTech", CatAltres, MPitch, 2, 0xffc8252a, W, F::Basculant,
                             "canvi de to amb pedal", "rock alternatiu metall", "Puja o baixa el to movent el pedal.",
                             { ped ("pedal", "Pedal", 0), sel ("mode", "Mode", { "+2 oct", "+1 oct", "+5a", "-1 oct", "-2 oct", "Harm. +1 oct", "Detune" }, 1) }));
            v.push_back (pd ("ns2", "Noise Gate N2", "Boss NS-2 Noise Suppressor", "Boss", CatAltres, MUtil, 0, 0xffececec, B, F::Compacte,
                             "porta de soroll", "metall rock alt guany", "Talla el brunzit entre notes.",
                             { db ("threshold", "Threshold", -80, -20, -55), ms ("decay", "Decay", 10, 500, 90) }));
            v.push_back (pd ("ge7", "Graphic EQ 7", "Boss GE-7 Equalizer", "Boss", CatAltres, MUtil, 1, 0xfff2f2f2, B, F::Compacte,
                             "equalitzador gràfic", "tots", "Equalitzador de set bandes (±15 dB) i nivell.",
                             { db ("b100", "100", -15, 15, 0), db ("b200", "200", -15, 15, 0), db ("b400", "400", -15, 15, 0), db ("b800", "800", -15, 15, 0),
                               db ("b1k6", "1.6k", -15, 15, 0), db ("b3k2", "3.2k", -15, 15, 0), db ("b6k4", "6.4k", -15, 15, 0), db ("level", "Level", -15, 15, 0) }));
            v.push_back (pd ("volume", "Volume Pedal", "Pedal de volum", "Genèric", CatAltres, MUtil, 2, 0xff242424, W, F::Basculant,
                             "volum, entrades suaus", "ambient post-rock country", "Volum amb el peu: per fer entrar les notes sense atac (violí).",
                             { ped ("pedal", "Pedal", 100), pom ("min", "Mínim", 0) }));
            return v;
        }

        // ---------------------------------------------------------------------------------------------- amplis
        Veu veu (float gMin, float gMax, int etapes, float biaix, float duresa, float midF, float midDb, float hpEtapa, float lpEtapa,
                 float potencia, float sag, float brillant = 0.0f, int posPila = 1, float lpSortida = 8500.0f)
        {
            Veu u;
            u.guanyMin = gMin; u.guanyMax = gMax; u.etapes = etapes; u.biaix = biaix; u.duresa = duresa; u.midF = midF; u.midDb = midDb;
            u.hpEtapa = hpEtapa; u.lpEtapa = lpEtapa; u.potencia = potencia; u.sag = sag; u.brillant = brillant; u.posPila = posPila; u.lpSortida = lpSortida;
            return u;
        }

        ModelAmp amp (const char* clau, const char* nom, const char* inspirat, const char* marca, const char* origen, const char* caracter, const char* estils,
                      const char* desc, int pila, bool teMaster, std::array<const char*, numAmpCtl> et, std::array<float, numAmpCtl> def,
                      std::vector<std::string> canals, std::vector<Veu> veus, const char* cab, const char* alt,
                      std::uint32_t tolex, std::uint32_t panell, std::uint32_t text, std::uint32_t reixa, int estilPom)
        {
            ModelAmp a;
            a.clau = clau; a.nom = nom; a.inspirat = inspirat; a.marca = marca; a.origen = origen; a.caracter = caracter; a.estils = estils; a.descripcio = desc;
            a.pila = pila; a.teMaster = teMaster;
            for (int i = 0; i < numAmpCtl; ++i) a.etiqueta[(size_t) i] = et[(size_t) i];
            a.def = def; a.canals = std::move (canals); a.veus = std::move (veus); a.cab = cab; a.altaveu = alt;
            a.tolex = tolex; a.panell = panell; a.textPanell = text; a.reixa = reixa; a.estilPom = estilPom;
            return a;
        }

        std::vector<ModelAmp> creaAmplis()
        {
            std::vector<ModelAmp> v;
            const char* const N = "";
            // ------------------------------------------------------------ britànics
            {
                auto a = amp ("plexi", "Plexi 59", "Marshall Super Lead 1959 «Plexi»", "Marshall", "Britànic", "crunch obert, mitjos, dinàmic", "rock clàssic blues hard rock",
                              "El crunch britànic per excel·lència: sense master, el volum fa cantar les vàlvules.", PMarshall, false,
                              { "Volume", "Bass", "Middle", "Treble", "Presence", N, N, N }, { 6, 5, 7, 6, 5, 0, 0, 0 },
                              { "Normal", "High Treble" },
                              { veu (-6, 26, 2, 0.30f, 1.2f, 700, 2, 30, 7000, 12, 0.35f, 0, 1),
                                veu (-4, 30, 2, 0.30f, 1.25f, 900, 3, 40, 7500, 12, 0.35f, 6, 1) },
                              "1960", "greenback", 0xff141414, 0xffd7b866, 0xff141414, 0xffcfc6b4, 0);
                a.canalDef = 1;
                v.push_back (a);
            }
            v.push_back (amp ("jcm800", "JC 800", "Marshall JCM800 2203", "Marshall", "Britànic", "rock dur, mitjos aguts, agressiu", "hard rock heavy punk 80s",
                              "El so del rock dels 80: un sol canal amb master i molt de mos.", PMarshall, true,
                              { "Pre-amp", "Bass", "Middle", "Treble", "Presence", "Master", N, N }, { 7, 5, 6, 6, 5, 5, 0, 0 }, {},
                              { veu (6, 44, 3, 0.32f, 1.4f, 1000, 3, 60, 6800, 8, 0.25f, 3, 2) },
                              "1960", "g12h", 0xff141414, 0xffd7b866, 0xff141414, 0xff2a2620, 0));
            {
                auto a = amp ("jcm900", "JC 900", "Marshall JCM900 4100", "Marshall", "Britànic", "guany alt, brillant, 90s", "hard rock grunge metall 90s",
                              "Més guany que el 800, amb un toc de reverb. Molt brillant.", PMarshall, true,
                              { "Gain", "Bass", "Middle", "Treble", "Presence", "Master", "Reverb", N }, { 6, 5, 5, 6, 5, 5, 2, 0 },
                              { "Clean", "Lead" },
                              { veu (-4, 22, 2, 0.25f, 1.1f, 900, 1, 30, 8000, 8, 0.25f, 3, 1),
                                veu (10, 50, 3, 0.33f, 1.6f, 1100, 3, 80, 6500, 8, 0.2f, 2, 2) },
                              "1960", "v30", 0xff141414, 0xffd7b866, 0xff141414, 0xff2a2620, 0);
                a.canalDef = 1;
                v.push_back (a);
            }
            {
                auto a = amp ("jcm2000", "DSL 2K", "Marshall JCM2000 DSL", "Marshall", "Britànic", "versàtil, crunch a lead", "rock hard rock indie",
                              "Dos canals (Classic Gain i Ultra Gain) amb Deep (greus) i Tone Shift (mitjos buidats).", PMarshall, true,
                              { "Gain", "Bass", "Middle", "Treble", "Presence", "Master", "Reverb", "Deep" }, { 6, 5, 6, 6, 5, 5, 0, 3 },
                              { "Classic Gain", "Ultra Gain" },
                              { veu (-2, 34, 2, 0.30f, 1.25f, 900, 2, 40, 7500, 8, 0.25f, 3, 1),
                                veu (12, 54, 3, 0.34f, 1.7f, 1000, 2, 90, 6400, 8, 0.2f, 2, 2) },
                              "1960", "v30", 0xff141414, 0xffd7b866, 0xff141414, 0xff2a2620, 0);
                a.extra = Extra::Depth; a.interruptor = Interruptor::ToneShift; a.nomInterruptor = "Tone Shift"; a.canalDef = 0;
                v.push_back (a);
            }
            {
                auto a = amp ("ac30", "AC 30 TB", "Vox AC30 Top Boost", "Vox", "Britànic", "brillant, «chime», classe A", "britpop indie rock clàssic 60s",
                              "La campana britànica: aguts que brillen i un crunch dolç amb el volum.", PVox, false,
                              { "Volume", "Bass", N, "Treble", "Cut", N, N, N }, { 5, 5, 0, 6, 4, 0, 0, 0 },
                              { "Normal", "Top Boost" },
                              { veu (-6, 22, 1, 0.22f, 1.0f, 1800, 1, 20, 9500, 14, 0.45f, 2, 1, 9000),
                                veu (-2, 30, 2, 0.24f, 1.05f, 2200, 3, 30, 9000, 14, 0.45f, 4, 2, 9000) },
                              "vox212", "greenback", 0xff1d1d1d, 0xff8a5a3a, 0xfff3e4c9, 0xff6a4a3a, 2);
                a.etiqueta[AMiddle] = ""; a.canalDef = 1;
                v.push_back (a);
            }
            v.push_back (amp ("hiwatt", "Hi-Watt 103", "Hiwatt DR103", "Hiwatt", "Britànic", "net molt fort, potent, definit", "rock clàssic indie post-rock",
                              "Net enorme i definit fins a volums molt alts: el so d'escenari dels 70.", PHiwatt, true,
                              { "Normal Vol", "Bass", "Middle", "Treble", "Presence", "Master", N, N }, { 5, 5, 5, 6, 5, 6, 0, 0 }, {},
                              { veu (-8, 24, 2, 0.18f, 0.95f, 1200, 1, 30, 9000, 6, 0.15f, 1, 1, 9500) },
                              "hiwatt", "creamback", 0xff111111, 0xff1b1b1b, 0xffe8e8e8, 0xff2c2a28, 4));
            // ------------------------------------------------------------ americans
            {
                auto a = amp ("twin", "Twin 65", "Fender Twin Reverb", "Fender", "Americà", "net cristal·lí, molt headroom", "net country indie surf funk",
                                  "El net de referència: brillant, ample i amb la reverb de molla de casa.", PFender, false,
                                  { "Volume", "Bass", "Middle", "Treble", N, N, "Reverb", N }, { 4, 5, 4, 6, 0, 0, 3, 0 }, {},
                                  { veu (-12, 16, 1, 0.12f, 0.9f, 500, -2, 15, 11000, 6, 0.3f, 4, 1, 10000) },
                                  "fender212", "c12n", 0xff141414, 0xff1a1a1a, 0xffdedede, 0xffc8c3b8, 1);
                a.interruptor = Interruptor::Bright; a.nomInterruptor = "Bright";
                v.push_back (a);
            }
            v.push_back (amp ("deluxe", "Deluxe 65", "Fender Deluxe Reverb", "Fender", "Americà", "net que s'esqueixa, càlid, petit", "blues indie country estudi",
                              "Petit i càlid: net a volum baix, s'esqueixa dolç quan el puges.", PFender, false,
                              { "Volume", "Bass", N, "Treble", N, N, "Reverb", N }, { 5, 5, 0, 6, 0, 0, 3, 0 }, {},
                              { veu (-8, 26, 2, 0.15f, 1.0f, 500, -1, 15, 10000, 12, 0.45f, 3, 1, 8500) },
                              "fender112", "c12n", 0xff141414, 0xff1a1a1a, 0xffdedede, 0xffc8c3b8, 1));
            {
                auto a = amp ("bassman", "Tweed Bass 59", "Fender Bassman 5F6-A", "Fender", "Americà", "tweed, gruixut, crunch vintage", "blues rock clàssic rockabilly",
                                  "L'avi de tots els amplis de rock: tweed, gras i amb un crunch que es menja la púa.", PFender, false,
                                  { "Volume", "Bass", "Middle", "Treble", "Presence", N, N, N }, { 6, 5, 6, 6, 5, 0, 0, 0 },
                                  { "Normal", "Bright" },
                                  { veu (-6, 28, 2, 0.20f, 1.05f, 600, 1, 20, 8000, 12, 0.5f, 0, 1, 7500),
                                    veu (-4, 30, 2, 0.20f, 1.05f, 700, 2, 20, 8500, 12, 0.5f, 5, 1, 7500) },
                                  "fender410", "c12n", 0xffc9b07a, 0xff6b4b2a, 0xfff3e7c8, 0xff7a5a3a, 1);
                a.canalDef = 1;
                v.push_back (a);
            }
            {
                auto a = amp ("superreverb", "Super 65", "Fender Super Reverb", "Fender", "Americà", "net gros, brillant", "blues net indie surf",
                                  "Net ample i brillant amb reverb, molt estimat pel blues.", PFender, false,
                                  { "Volume", "Bass", "Middle", "Treble", N, N, "Reverb", N }, { 5, 5, 5, 6, 0, 0, 3, 0 }, {},
                                  { veu (-10, 20, 2, 0.13f, 0.95f, 550, -1, 15, 10500, 8, 0.35f, 4, 1, 9500) },
                                  "fender410", "c12n", 0xff141414, 0xff1a1a1a, 0xffdedede, 0xffc8c3b8, 1);
                a.interruptor = Interruptor::Bright; a.nomInterruptor = "Bright";
                v.push_back (a);
            }
            {
                auto a = amp ("rectifier", "Recto Dual", "Mesa/Boogie Dual Rectifier", "Mesa/Boogie", "Americà", "guany molt alt, greus enormes", "metall nu-metal grunge modern",
                                  "Greus enormes i guany a dojo: el so del metall modern.", PMesa, true,
                                  { "Gain", "Bass", "Mid", "Treble", "Presence", "Master", N, N }, { 7, 6, 4, 6, 5, 5, 0, 0 },
                                  { "Clean", "Vintage", "Modern" },
                                  { veu (-8, 20, 2, 0.15f, 1.0f, 600, -1, 20, 9500, 6, 0.4f, 2, 1),
                                    veu (12, 50, 3, 0.32f, 1.6f, 800, 1, 100, 6500, 8, 0.45f, 1, 3),
                                    veu (18, 62, 4, 0.36f, 1.9f, 700, 0, 140, 6000, 9, 0.5f, 1, 4) },
                                  "mesa412", "v30", 0xff151515, 0xff2b2b2b, 0xffe0e0e0, 0xff1e1e1e, 3);
                a.canalDef = 2;
                v.push_back (a);
            }
            {
                auto a = amp ("markiic", "Mark Two C", "Mesa/Boogie Mark IIC+", "Mesa/Boogie", "Americà", "lead cantant, mitjos, compacte", "metall thrash rock solo fusió",
                                  "El lead més cantant i precís: el Graphic V buida els mitjos com el famós EQ en V.", PMesa, true,
                                  { "Volume", "Bass", "Middle", "Treble", "Presence", "Master", "Reverb", "Graphic V" }, { 6, 4, 5, 7, 5, 5, 0, 4 },
                                  { "Rhythm", "Lead" },
                                  { veu (-6, 24, 2, 0.16f, 1.0f, 650, 1, 25, 9500, 6, 0.25f, 3, 1),
                                    veu (14, 58, 4, 0.34f, 1.8f, 700, 4, 120, 6200, 7, 0.25f, 1, 1) },
                                  "mesa112", "evm12l", 0xff151515, 0xff222222, 0xffe0e0e0, 0xff2a2a2a, 3);
                a.extra = Extra::GraficV; a.canalDef = 1;
                v.push_back (a);
            }
            {
                auto a = amp ("5150", "Fifty-150", "Peavey 5150", "Peavey", "Americà", "guany molt alt, agressiu, ajustat", "metall metalcore hard rock",
                                  "Agressiu i ajustat: el rei del metall dels 90 i 2000.", PModern, true,
                                  { "Pre Gain", "Low", "Mid", "High", "Presence", "Post Gain", N, "Resonance" }, { 6, 6, 5, 6, 5, 5, 0, 5 },
                                  { "Rhythm", "Lead" },
                                  { veu (2, 36, 2, 0.28f, 1.3f, 800, 2, 50, 7500, 6, 0.2f, 3, 1),
                                    veu (20, 62, 4, 0.36f, 2.0f, 850, 2, 150, 6000, 7, 0.2f, 1, 4) },
                                  "mesa412", "v30", 0xff121212, 0xff1a1a1a, 0xffd0d0d0, 0xff2b2b2b, 4);
                a.extra = Extra::Resonance; a.canalDef = 1;
                v.push_back (a);
            }
            {
                auto a = amp ("slo", "Solo 100", "Soldano SLO-100", "Soldano", "Americà", "lead suau, cremós, britànic modificat", "hard rock solo blues rock",
                                  "Lead cremós i suau, amb un crunch preciós al canal normal.", PMarshall, true,
                                  { "Preamp", "Bass", "Middle", "Treble", "Presence", "Master", N, "Depth" }, { 6, 5, 6, 6, 5, 5, 0, 4 },
                                  { "Clean", "Crunch", "Overdrive" },
                                  { veu (-8, 20, 2, 0.16f, 1.0f, 700, 0, 25, 9500, 6, 0.25f, 3, 1),
                                    veu (4, 38, 3, 0.30f, 1.3f, 900, 3, 60, 7500, 7, 0.25f, 2, 2),
                                    veu (14, 58, 4, 0.33f, 1.6f, 950, 3, 100, 6800, 7, 0.25f, 1, 3) },
                                  "1960", "v30", 0xff151515, 0xffc8c8cc, 0xff111111, 0xff1f1f1f, 3);
                a.extra = Extra::Depth; a.canalDef = 2;
                v.push_back (a);
            }
            // ------------------------------------------------------------ moderns i d'alt guany
            {
                auto a = amp ("powerball", "Power Bowl", "ENGL Powerball", "ENGL", "Modern", "guany alt, greus ajustats, modern", "metall modern djent hard rock",
                                  "Alemany i precís: guany alt amb els greus molt controlats.", PModern, true,
                                  { "Gain", "Bass", "Middle", "Treble", "Presence", "Master", N, "Depth" }, { 6, 5, 5, 6, 5, 5, 0, 4 },
                                  { "Clean", "Crunch", "Lead", "Ultra" },
                                  { veu (-8, 18, 2, 0.15f, 1.0f, 700, 0, 25, 9500, 6, 0.2f, 2, 1),
                                    veu (6, 38, 3, 0.28f, 1.3f, 800, 1, 70, 7500, 7, 0.2f, 2, 2),
                                    veu (14, 54, 4, 0.33f, 1.7f, 800, 1, 130, 6500, 7, 0.2f, 1, 4),
                                    veu (20, 64, 4, 0.36f, 2.1f, 750, 0, 170, 6000, 7, 0.2f, 1, 4) },
                                  "mesa412", "v30", 0xff151515, 0xff1d1d1f, 0xffe8e8e8, 0xff1b1b1b, 4);
                a.extra = Extra::Depth; a.interruptor = Interruptor::Tight; a.nomInterruptor = "Tight"; a.canalDef = 2;
                v.push_back (a);
            }
            {
                auto a = amp ("vh4", "Diesel 4", "Diezel VH4", "Diezel", "Modern", "guany alt, definit, gruixut", "metall rock alternatiu progressiu",
                                  "Quatre canals; el tercer i el quart són un mur definit i molt gruixut.", PModern, true,
                                  { "Gain", "Bass", "Middle", "Treble", "Presence", "Master", N, "Deep" }, { 6, 5, 5, 6, 5, 5, 0, 4 },
                                  { "Ch 1", "Ch 2", "Ch 3", "Ch 4" },
                                  { veu (-8, 18, 2, 0.14f, 1.0f, 700, 0, 25, 9500, 6, 0.2f, 2, 1),
                                    veu (6, 40, 3, 0.28f, 1.35f, 900, 2, 70, 7500, 7, 0.2f, 2, 2),
                                    veu (14, 56, 4, 0.33f, 1.7f, 900, 2, 120, 6500, 7, 0.2f, 1, 4),
                                    veu (20, 64, 4, 0.35f, 2.0f, 850, 3, 150, 6200, 7, 0.2f, 1, 4) },
                                  "mesa412", "v30", 0xff161616, 0xff7c7f84, 0xff111111, 0xff1d1d1d, 3);
                a.extra = Extra::Depth; a.canalDef = 2;
                v.push_back (a);
            }
            {
                auto a = amp ("ecstasy", "Exstasy", "Bogner Ecstasy", "Bogner", "Modern", "crunch i lead rics, harmònics", "hard rock rock clàssic fusió",
                                  "Ric en harmònics: el canal Blue és crunch clàssic i el Red, lead modern.", PMarshall, true,
                                  { "Gain", "Bass", "Middle", "Treble", "Presence", "Master", N, "Depth" }, { 6, 5, 6, 6, 5, 5, 0, 4 },
                                  { "Clean", "Blue", "Red" },
                                  { veu (-8, 20, 2, 0.15f, 1.0f, 700, 0, 20, 10000, 6, 0.25f, 3, 1),
                                    veu (4, 40, 3, 0.29f, 1.3f, 900, 2, 60, 7500, 7, 0.25f, 2, 2),
                                    veu (14, 56, 4, 0.33f, 1.65f, 950, 2, 110, 6800, 7, 0.25f, 1, 3) },
                                  "mesa412", "v30", 0xff171717, 0xff1b2a4a, 0xffd9d9d9, 0xff2a2a2a, 3);
                a.interruptor = Interruptor::Boost; a.nomInterruptor = "Boost"; a.extra = Extra::Depth; a.canalDef = 1;
                v.push_back (a);
            }
            {
                auto a = amp ("rockerverb", "Rocker Verb", "Orange Rockerverb", "Orange", "Modern", "gras, mitjos, stoner", "stoner indie rock alternatiu doom",
                                  "Mitjos plens i greus grassos: el taronja de l'stoner i l'indie.", POrange, true,
                                  { "Gain", "Bass", "Middle", "Treble", N, "Volume", "Reverb", N }, { 6, 6, 6, 5, 0, 5, 2, 0 },
                                  { "Clean", "Dirty" },
                                  { veu (-8, 20, 2, 0.16f, 1.0f, 600, 1, 20, 9000, 8, 0.3f, 2, 1),
                                    veu (12, 54, 3, 0.33f, 1.6f, 600, 4, 70, 6200, 8, 0.3f, 1, 3) },
                                  "orange412", "v30", 0xffe8792a, 0xfff0e5d0, 0xff1b1b1b, 0xff2a2620, 2);
                a.canalDef = 1;
                v.push_back (a);
            }
            {
                auto a = amp ("evh5150iii", "Fifty-Fifty III", "EVH 5150 III", "EVH", "Modern", "guany alt, marró, modern", "hard rock metall rock clàssic",
                                  "Tres canals; el «brown sound» del tercer és calent i definit alhora.", PModern, true,
                                  { "Gain", "Low", "Mid", "High", "Presence", "Volume", N, "Resonance" }, { 6, 5, 5, 6, 5, 5, 0, 5 },
                                  { "Clean", "Crunch", "Lead" },
                                  { veu (-8, 18, 2, 0.14f, 1.0f, 650, 0, 20, 9500, 6, 0.2f, 2, 1),
                                    veu (8, 44, 3, 0.29f, 1.35f, 850, 2, 70, 7200, 7, 0.2f, 2, 2),
                                    veu (18, 60, 4, 0.35f, 1.9f, 850, 2, 130, 6300, 7, 0.2f, 1, 4) },
                                  "mesa412", "g12h", 0xff111111, 0xff111111, 0xffe8e8e8, 0xff222222, 4);
                a.extra = Extra::Resonance; a.canalDef = 2;
                v.push_back (a);
            }
            // Volum igualat entre models i canals (mesurat amb un rasgueig i notes soles, amb els poms de sempre).
            // Els canals nets no pugen més de +2 dB, perquè tenen més pics.
            static const std::pair<const char*, std::vector<float>> nivells[] = {
{ "plexi", { -4.8f, -6.8f, } },
{ "jcm800", { -5.3f, } },
{ "jcm900", { 1.5f, -5.7f, } },
{ "jcm2000", { -2.6f, -5.7f, } },
{ "ac30", { -1.5f, -4.7f, } },
{ "hiwatt", { 1.1f, } },
{ "twin", { 2.0f, } },
{ "deluxe", { -1.2f, } },
{ "bassman", { -3.9f, -5.4f, } },
{ "superreverb", { 0.9f, } },
{ "rectifier", { 2.0f, -3.0f, -2.8f, } },
{ "markiic", { 1.7f, -2.4f, } },
{ "5150", { -3.1f, -5.5f, } },
{ "slo", { 2.0f, -3.9f, -5.5f, } },
{ "powerball", { 2.0f, -3.4f, -5.2f, -5.5f, } },
{ "vh4", { 2.0f, -3.8f, -5.3f, -5.4f, } },
{ "ecstasy", { 2.0f, -3.3f, -4.4f, } },
{ "rockerverb", { 2.0f, -4.9f, } },
{ "evh5150iii", { 2.0f, -4.3f, -5.5f, } },
            };
            for (auto& a : v)
                for (const auto& [clau, ns] : nivells)
                    if (a.clau == clau)
                        for (size_t c = 0; c < a.veus.size() && c < ns.size(); ++c) a.veus[c].nivell = ns[c];
            return v;
        }

        ModelCab cab (const char* clau, const char* nom, const char* inspirat, int n, bool obert, float resF, float resDb, float resQ, float hp, float cosDb,
                      const char* alt, std::uint32_t tolex, std::uint32_t reixa)
        {
            ModelCab c;
            c.clau = clau; c.nom = nom; c.inspirat = inspirat; c.altaveus = n; c.obert = obert; c.resF = resF; c.resDb = resDb; c.resQ = resQ;
            c.hp = hp; c.cosDb = cosDb; c.altaveu = alt; c.tolex = tolex; c.reixa = reixa;
            return c;
        }

        std::vector<ModelCab> creaCabs()
        {
            return {
                cab ("1960", "Brit 4x12", "Marshall 1960 4x12", 4, false, 100, 4.5f, 1.5f, 70, 1.0f, "greenback", 0xff141414, 0xffcfc6b4),
                cab ("orange412", "Orange 4x12", "Orange PPC412", 4, false, 90, 5.0f, 1.3f, 65, 2.0f, "v30", 0xffe8792a, 0xff1f1b18),
                cab ("mesa412", "Recto 4x12", "Mesa Rectifier 4x12", 4, false, 85, 6.0f, 1.2f, 60, 1.5f, "v30", 0xff151515, 0xff1c1c1c),
                cab ("fender212", "US 2x12", "Fender 2x12 (Twin)", 2, true, 120, 2.0f, 1.0f, 90, -1.0f, "c12n", 0xff141414, 0xffc8c3b8),
                cab ("vox212", "Brit 2x12", "Vox 2x12 (AC30)", 2, true, 110, 2.5f, 1.0f, 85, 0.0f, "greenback", 0xff1d1d1d, 0xff6a4a3a),
                cab ("fender112", "US 1x12", "Fender 1x12 (Deluxe)", 1, true, 130, 2.0f, 1.0f, 100, -1.5f, "c12n", 0xff141414, 0xffc8c3b8),
                cab ("hiwatt", "Hi 4x12", "Hiwatt 4x12", 4, false, 95, 3.5f, 1.3f, 70, 0.5f, "creamback", 0xff111111, 0xff2c2a28),
                cab ("fender410", "Tweed 4x10", "Fender Bassman 4x10 (combo)", 4, true, 125, 2.0f, 1.1f, 95, -0.5f, "c12n", 0xffc9b07a, 0xff7a5a3a),
                cab ("mesa112", "Mark 1x12", "Mesa/Boogie 1x12 (combo)", 1, false, 115, 3.0f, 1.2f, 85, 0.5f, "evm12l", 0xff151515, 0xff2a2a2a),
            };
        }

        ModelAltaveu alt (const char* clau, const char* nom, const char* inspirat, const char* caracter, float lp, float p1F, float p1Db, float p1Q,
                          float p2F, float p2Db, float p2Q, float greus)
        {
            ModelAltaveu a;
            a.clau = clau; a.nom = nom; a.inspirat = inspirat; a.caracter = caracter; a.lp = lp; a.pic1F = p1F; a.pic1Db = p1Db; a.pic1Q = p1Q;
            a.pic2F = p2F; a.pic2Db = p2Db; a.pic2Q = p2Q; a.greusDb = greus;
            return a;
        }

        std::vector<ModelAltaveu> creaAltaveus()
        {
            return {
                alt ("v30", "Vintage 30", "Celestion Vintage 30", "mitjos aguts marcats, agressiu", 5200, 2500, 5.0f, 1.3f, 1200, -2.0f, 1.2f, 1.0f),
                alt ("greenback", "Greenback", "Celestion Greenback (G12M)", "càlid, mitjos dolços, vintage", 4600, 1600, 4.0f, 1.0f, 400, 1.5f, 0.9f, 0.0f),
                alt ("creamback", "Creamback", "Celestion Creamback (G12M-65)", "Greenback més ajustat i amb més headroom", 5000, 2000, 3.5f, 1.0f, 450, 1.0f, 0.9f, 1.0f),
                alt ("g12h", "G12H", "Celestion G12H", "greus ferms, aguts oberts", 6000, 3000, 3.0f, 1.0f, 110, 3.0f, 1.0f, 2.0f),
                alt ("c12n", "Jensen C12", "Jensen C12N", "brillant, net, americà", 6500, 3500, 4.0f, 1.0f, 500, -3.0f, 1.0f, -0.5f),
                alt ("evm12l", "EV 12L", "Electro-Voice EVM12L", "pla, potent, molt headroom", 7000, 2000, 1.0f, 0.8f, 80, 2.0f, 0.9f, 2.0f),
            };
        }

        ModelMicro mic (const char* clau, const char* nom, const char* inspirat, const char* caracter, float prox, float presF, float presDb, float presQ,
                        float aguts, float lp, float hp)
        {
            ModelMicro m;
            m.clau = clau; m.nom = nom; m.inspirat = inspirat; m.caracter = caracter; m.proximitat = prox; m.presF = presF; m.presDb = presDb;
            m.presQ = presQ; m.agutsDb = aguts; m.lp = lp; m.hp = hp;
            return m;
        }

        std::vector<ModelMicro> creaMicros()
        {
            return {
                mic ("sm57", "Dynamic 57", "Shure SM57", "dinàmic, presència als 6 kHz, el de sempre", 3.0f, 6000, 4.0f, 1.3f, 0.0f, 14000, 60),
                mic ("md421", "Dynamic 421", "Sennheiser MD421", "dinàmic, més ple i greus rodons", 4.0f, 4000, 2.5f, 0.8f, 0.0f, 15000, 45),
                mic ("r121", "Ribbon 121", "Royer R-121", "cinta, fosc i suau, greus generosos", 6.0f, 1500, 1.0f, 0.7f, -6.0f, 12000, 35),
                mic ("u87", "Condenser 87", "Neumann U87", "condensador, pla i obert, aire", 2.0f, 10000, 2.0f, 0.7f, 1.0f, 18000, 35),
            };
        }
    }

    const std::vector<ModelPedal>& pedals() { static const auto v = creaPedals(); return v; }
    const std::vector<ModelAmp>& amplis() { static const auto v = creaAmplis(); return v; }
    const std::vector<ModelCab>& cabs() { static const auto v = creaCabs(); return v; }
    const std::vector<ModelAltaveu>& altaveus() { static const auto v = creaAltaveus(); return v; }
    const std::vector<ModelMicro>& micros() { static const auto v = creaMicros(); return v; }

    int indexPedal (const std::string& clau)
    {
        const auto& v = pedals();
        for (size_t i = 0; i < v.size(); ++i) if (v[i].clau == clau) return (int) i;
        return -1;
    }
    int indexAmpli (const std::string& clau)
    {
        const auto& v = amplis();
        for (size_t i = 0; i < v.size(); ++i) if (v[i].clau == clau) return (int) i;
        return -1;
    }
    int indexCab (const std::string& clau)
    {
        const auto& v = cabs();
        for (size_t i = 0; i < v.size(); ++i) if (v[i].clau == clau) return (int) i;
        return -1;
    }
    int indexAltaveu (const std::string& clau)
    {
        const auto& v = altaveus();
        for (size_t i = 0; i < v.size(); ++i) if (v[i].clau == clau) return (int) i;
        return -1;
    }

    std::string senseAccents (const std::string& s)
    {
        static const std::pair<const char*, char> t[] = {
            { "à", 'a' }, { "á", 'a' }, { "è", 'e' }, { "é", 'e' }, { "í", 'i' }, { "ï", 'i' }, { "ò", 'o' }, { "ó", 'o' }, { "ú", 'u' }, { "ü", 'u' },
            { "ç", 'c' }, { "ñ", 'n' }, { "À", 'a' }, { "È", 'e' }, { "É", 'e' }, { "Í", 'i' }, { "Ò", 'o' }, { "Ó", 'o' }, { "Ú", 'u' }, { "Ç", 'c' }, { "·", '.' } };
        std::string r;
        for (size_t i = 0; i < s.size();)
        {
            bool fet = false;
            for (const auto& [a, b] : t)
            {
                const size_t n = std::char_traits<char>::length (a);
                if (s.compare (i, n, a) == 0) { r += b; i += n; fet = true; break; }
            }
            if (! fet) { r += (char) std::tolower ((unsigned char) s[i]); ++i; }
        }
        return r;
    }

    std::string textCerca (const ModelPedal& p)
    {
        return senseAccents (p.nom + " " + p.inspirat + " " + p.marca + " " + nomCategoria (p.categoria) + " " + p.caracter + " " + p.estils);
    }
    std::string textCerca (const ModelAmp& a)
    {
        return senseAccents (a.nom + " " + a.inspirat + " " + a.marca + " ampli amplificador " + a.origen + " " + a.caracter + " " + a.estils);
    }
}
