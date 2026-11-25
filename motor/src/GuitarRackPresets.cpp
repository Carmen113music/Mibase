#include "mibase/GuitarRack.h"

#include <sstream>

// 1.5: els presets de fàbrica del Guitar Rack (cadenes senceres: pedals, ampli, pantalla i micròfons).
// Llenguatge curt: «amp=… clau=valor … | pedal [loop] [off] control=valor … | …».
// Al primer tros: amp, cab, alt i qualsevol paràmetre fix (gain, bass, canal, mica, posa, room…).
namespace mibase::guitarrack
{
    std::string construeix (const std::string& descripcio)
    {
        GuitarRack r;
        r.buidaPlaca();
        std::vector<std::string> trossos;
        {
            std::string t;
            for (char ch : descripcio + "|")
            {
                if (ch == '|') { trossos.push_back (t); t.clear(); }
                else t += ch;
            }
        }
        auto parells = [] (const std::string& s)
        {
            std::vector<std::pair<std::string, std::string>> v;
            std::istringstream is (s);
            std::string w;
            while (is >> w)
            {
                const auto e = w.find ('=');
                if (e == std::string::npos) v.push_back ({ w, "" });
                else v.push_back ({ w.substr (0, e), w.substr (e + 1) });
            }
            return v;
        };
        for (size_t t = 0; t < trossos.size(); ++t)
        {
            const auto ps = parells (trossos[t]);
            if (ps.empty()) continue;
            if (t == 0)
            {
                for (auto& [k, v] : ps) if (k == "amp") r.posaAmpli (std::max (0, indexAmpli (v)), true);
                for (auto& [k, v] : ps) if (k == "cab") r.posaCab (std::max (0, indexCab (v)), true);
                for (auto& [k, v] : ps)
                {
                    if (k == "amp" || k == "cab") continue;
                    if (k == "alt") { r.posaAltaveu (std::max (0, indexAltaveu (v))); continue; }
                    for (int p = 0; p < numFixos; ++p) if (k == infoFix (p).clau) r.posa (p, std::stof (v));
                }
                continue;
            }
            const std::string clau = ps[0].first;
            bool loop = false, off = false;
            for (auto& [k, v] : ps) { if (k == "loop") loop = true; if (k == "off") off = true; }
            const int k = r.afegeix (clau, -1, loop);
            auto* p = r.pedal (k);
            if (p == nullptr) continue;
            p->on.store (! off);
            for (auto& [kk, v] : ps)
            {
                if (v.empty()) continue;
                for (int c = 0; c < p->numControls(); ++c) if (p->model().controls[(size_t) c].clau == kk) p->posa (c, std::stof (v));
            }
        }
        return r.estat();
    }

    const std::vector<std::string>& categories()
    {
        static const std::vector<std::string> c = { "Net", "Crunch", "Rock", "Alt guany", "Ambient", "Indie", "Estils" };
        return c;
    }

    namespace
    {
        struct Def { const char* nom; const char* cat; const char* desc; const char* cadena; };
        const Def defs[] = {
            // ---------------------------------------------------------------- Net
            { "Clean Fender", "Net", "El net americà de referència: brillant, ample i amb molla.",
              "amp=twin gain=4 bass=5 middle=4 treble=6 reverb=3 cab=fender212 mica=0 posa=0.35 dista=0.15 room=12 | keeley sustain=3 level=5 blend=6" },
            { "Indie Rock Clean", "Net", "Net britànic amb campana, compressor suau i una plate curta.",
              "amp=ac30 canal=0 gain=4 bass=5 treble=6 presence=4 cab=vox212 mica=0 posa=0.4 micb=2 mesclab=30 room=15 | keeley sustain=4 blend=6 | plate loop decay=3 mix=2.5" },
            { "Dreamy Chorus", "Net", "Net amb chorus càlid, eco digital i hall: per a arpegis.",
              "amp=twin gain=3.5 treble=6.5 reverb=1 cab=fender212 room=15 | cs3 sustain=4 | ce2 rate=2.5 depth=6 | dd3 loop level=3.5 feedback=4 time=420 | rv6 loop level=4 time=6 mode=1" },
            { "Funky Clean", "Net", "Net sec i percussiu amb compressor de pedal i una mica de phaser.",
              "amp=twin gain=3 bass=4 middle=5 treble=7 bright=1 cab=fender212 room=8 | dynacomp output=6 sens=6 | phase90 off speed=3" },
            { "Jazz Box", "Net", "Net rodó i fosc per a acords amb setenes.",
              "amp=deluxe gain=3 bass=6 treble=3.5 reverb=2 cab=fender112 mica=2 posa=0.5 dista=0.2 room=18" },
            { "Country Snap", "Net", "Net brillant amb compressor i slapback curt.",
              "amp=twin gain=4 bass=4 middle=5 treble=7 bright=1 cab=fender212 room=8 | dynacomp output=6 sens=5 | dd3 loop level=4 feedback=1 time=110" },
            { "Surf 62", "Net", "Molla de caixa que degota i trèmolo a punt.",
              "amp=deluxe gain=4.5 bass=5 treble=6.5 cab=fender112 room=10 | fenderspring dwell=7 mix=5 tone=6 | tr2 off rate=6 wave=3 depth=6" },
            // ---------------------------------------------------------------- Crunch
            { "British Crunch", "Crunch", "El crunch d'un Plexi amb el volum alt: obert i dinàmic.",
              "amp=plexi canal=1 gain=6.5 bass=4 middle=7 treble=6 presence=5 cab=1960 alt=greenback mica=0 posa=0.3 room=12" },
            { "Classic Rock Marshall", "Crunch", "Plexi empès amb un Screamer i un eco analògic fosc.",
              "amp=plexi canal=1 gain=7 bass=4 middle=7 treble=6.5 presence=6 cab=1960 alt=greenback micb=1 mesclab=25 room=12 | ts808 drive=2 tone=6 level=7 | carbon loop regen=3 mix=3 delay=380" },
            { "Vintage Blues", "Crunch", "Tweed que s'esqueixa amb un Centaur per empènyer els solos.",
              "amp=bassman canal=1 gain=5.5 bass=4 middle=6 treble=6 presence=5 cab=fender410 room=15 | klon gain=3 treble=5.5 output=6.5 | rv6 loop level=3 time=4 mode=3" },
            { "Indie Rock Crunch", "Crunch", "Top Boost amb un Blues Driver: crunch brillant de guitarres indie.",
              "amp=ac30 canal=1 gain=5 bass=5 treble=6 presence=3 cab=vox212 mica=0 posa=0.35 room=12 | bd2 gain=4 tone=5.5 level=6 | plate loop decay=3 mix=2" },
            { "Tweed Breakup", "Crunch", "Un Deluxe gairebé al màxim: el crunch més natural.",
              "amp=deluxe gain=7.5 bass=4.5 treble=6 reverb=2 cab=fender112 mica=0 posa=0.4 room=15" },
            { "Radio Rock", "Crunch", "Rock de ràdio: JC 800 net de fang, mitjos al seu lloc.",
              "amp=jcm800 gain=5.5 bass=4.5 middle=6 treble=6 presence=5 master=5 cab=1960 alt=v30 mica=0 posa=0.3 micb=1 mesclab=30 room=10 | ts9 drive=1.5 tone=6 level=7 | dd3 loop level=2.5 feedback=2 time=330" },
            // ---------------------------------------------------------------- Rock
            { "Stadium Rock", "Rock", "Gran i ample: guany mitjà, delay amb tempo i plate.",
              "amp=jcm800 gain=7 bass=5 middle=6 treble=6 presence=6 master=6 cab=1960 alt=v30 micb=2 mesclab=35 room=20 | ts808 drive=2 tone=6 level=7 | timeline loop type=1 repeats=4 mix=3.5 tempo=2 | plate loop decay=5 mix=3" },
            { "80s Lead", "Rock", "Lead dels 80: Screamer, chorus al loop, delay i hall.",
              "amp=jcm800 gain=8 bass=5 middle=6.5 treble=6.5 presence=6 master=6 cab=1960 alt=v30 room=18 | ts808 drive=1.5 tone=6.5 level=8 | ce2 loop rate=2 depth=4 | dd7 loop level=4 feedback=4 time=420 mode=2 | rv6 loop level=4 time=6 mode=1" },
            { "Heavy Rock", "Rock", "Rock dur i gruixut amb un 5150 al canal Rhythm.",
              "amp=5150 canal=0 gain=7 bass=6 middle=5 treble=6 presence=5 master=5 cab=mesa412 alt=v30 mica=0 posa=0.25 micb=1 mesclab=30 room=8 gate=-68 | ts9 drive=1 tone=6 level=7" },
            { "Hard Rock Lead", "Rock", "Lead cremós d'un Solo 100 amb un eco darrere.",
              "amp=slo canal=2 gain=6.5 bass=5 middle=6 treble=6 presence=5 master=5 cab=1960 alt=v30 room=15 | dd3 loop level=3.5 feedback=4 time=450 | plate loop decay=4 mix=2" },
            { "Brown Sound", "Rock", "El «brown sound» calent amb un phaser lent.",
              "amp=evh5150iii canal=2 gain=6 bass=5 middle=6 treble=6 presence=5 master=5 cab=mesa412 alt=g12h room=18 | phase90 speed=2 | plate loop decay=4 mix=2.5" },
            { "Modern Alternative", "Rock", "Recto Vintage amb una RAT: alternatiu modern, gruixut però clar.",
              "amp=rectifier canal=1 gain=5.5 bass=5 middle=5 treble=6 presence=5 master=5 cab=mesa412 alt=v30 micb=2 mesclab=25 room=10 gate=-70 | rat dist=4 filter=4 volume=6" },
            // ---------------------------------------------------------------- Alt guany
            { "Modern Metal", "Alt guany", "Ajustat i precís: Power Bowl, gate i un Screamer de «tight».",
              "amp=powerball canal=2 gain=6.5 bass=6 middle=5 treble=6 presence=6 master=5 extra=5 bright=1 cab=mesa412 alt=v30 mica=0 posa=0.2 dista=0.05 room=6 gate=-62 | ns2 threshold=-60 decay=60 | ts9 drive=0 tone=7 level=8" },
            { "Thrash Mark", "Alt guany", "Lead i ritme amb el Graphic V en V.",
              "amp=markiic canal=1 gain=6 bass=4 middle=5 treble=7 presence=5 master=5 extra=6 cab=mesa112 alt=evm12l mica=0 posa=0.25 room=6 gate=-62 | ns2 threshold=-58" },
            { "Nu Wall", "Alt guany", "Recto Modern: un mur de greus.",
              "amp=rectifier canal=2 gain=6.5 bass=6 middle=4 treble=6 presence=5 master=5 cab=mesa412 alt=v30 micb=1 mesclab=40 room=6 gate=-60 | ns2 threshold=-56 decay=70" },
            { "Diesel Wall", "Alt guany", "El quart canal del Diesel 4 amb un Super Drive davant.",
              "amp=vh4 canal=3 gain=6 bass=5 middle=6 treble=6 presence=5 master=5 cab=mesa412 alt=v30 room=8 gate=-62 | ns2 threshold=-60 | sd1 drive=1 tone=6 level=7" },
            // ---------------------------------------------------------------- Ambient
            { "Shoegaze Atmosphere", "Ambient", "Fuzz, chorus profund, eco fosc i un núvol de reverb.",
              "amp=twin gain=5 bass=5 middle=5 treble=5.5 reverb=0 cab=fender212 room=25 | muff sustain=7 tone=4 volume=5 | smallclone rate=3 depth=1 | memoryman loop level=5 blend=5 feedback=6 delay=420 depth=5 | bigsky loop type=5 decay=8 mix=6 tone=4 mod=6" },
            { "Ambient Guitar", "Ambient", "Entrades amb el volum, ecos modulats i shimmer.",
              "amp=deluxe gain=4 bass=5 treble=5 cab=fender112 mica=2 room=25 | volume pedal=100 | dd7 loop level=5 feedback=6 time=600 mode=2 | bigsky loop type=6 decay=8 mix=5 tone=5 mod=4" },
            { "Post-Rock", "Ambient", "Crescendos: net-crunch amb delay amb tempo i hall llarg.",
              "amp=ac30 canal=1 gain=5 bass=5 treble=6 presence=4 cab=vox212 room=20 | bd2 off gain=6 level=6 | timeline loop type=0 repeats=5 mix=4 filter=5 tempo=2 | bigsky loop type=1 decay=7 mix=4" },
            { "Dub Space", "Ambient", "Eco de cinta que s'acumula i molla.",
              "amp=twin gain=4 treble=6 cab=fender212 room=10 | spaceecho loop rate=5 intensity=6 echo=6 mode=3 reverb=3" },
            { "Vibe Machine", "Ambient", "El vaivé psicodèlic davant d'un tweed.",
              "amp=bassman canal=1 gain=5 middle=6 cab=fender410 room=15 | univibe speed=4 intensity=7 mode=0 | rv6 loop level=3 time=5 mode=3" },
            // ---------------------------------------------------------------- Indie
            { "Britpop 95", "Indie", "Top Boost amb un Obsessive Drive: guitarres de tornada britàniques.",
              "amp=ac30 canal=1 gain=5.5 bass=5 treble=6.5 presence=3 cab=vox212 room=12 | ocd volume=6 drive=4 tone=6 mode=1 | plate loop decay=3 mix=2" },
            { "Garage Fuzz", "Indie", "Brut i senzill: Distortion Plus en un Deluxe.",
              "amp=deluxe gain=6.5 bass=5 treble=6 cab=fender112 room=12 | distplus dist=7 output=6" },
            { "Octave Fuzz", "Indie", "Fuzz amb l'octava baixa: riffs gruixuts a una corda.",
              "amp=rockerverb canal=1 gain=4 bass=5 middle=6 treble=5 master=5 cab=orange412 room=10 | oc2 oct1=6 oct2=2 direct=7 | muff sustain=6 tone=5 volume=5" },
            { "Wah Rhythm", "Indie", "Wah per a ritmes funky (automatitza el pedal).",
              "amp=twin gain=4 treble=6.5 cab=fender212 room=8 | crybaby pedal=45 | dynacomp output=5 sens=4" },
            // ---------------------------------------------------------------- Estils
            { "Desert Stoner", "Estils", "Orange calent amb un fuzz enorme: stoner del desert.",
              "amp=rockerverb canal=1 gain=6 bass=6 middle=7 treble=5 master=5 cab=orange412 alt=v30 room=10 | muff sustain=6 tone=4 volume=6" },
            { "Seattle 92", "Estils", "Grunge: Rodent, JC 900 i molt de greu.",
              "amp=jcm900 canal=1 gain=5 bass=6 middle=5 treble=6 presence=5 master=5 cab=1960 alt=v30 room=8 | rat dist=6 filter=4 volume=6 | smallclone off rate=3" },
            { "Madchester", "Estils", "Wah, chorus i un crunch net per ballar.",
              "amp=twin gain=5 treble=6 bright=1 cab=fender212 room=12 | crybaby pedal=60 | ce2 rate=2 depth=5 | rv6 loop level=3 time=4 mode=1" },
            { "Whammy Dive", "Estils", "Un Whammy d'octava amb una distorsió moderna.",
              "amp=5150 canal=1 gain=5.5 bass=5 middle=6 treble=6 cab=mesa412 alt=v30 room=8 gate=-62 | whammy pedal=0 mode=1 | ns2 threshold=-58" },
        };
    }

    /** El volum de sortida de cada preset (dB), mesurat perquè tots sonin semblant de forts sense passar-se de pics. */
    const std::pair<const char*, float> nivellsPreset[] = {
{ "Clean Fender", -2.4f },
{ "Indie Rock Clean", 3.6f },
{ "Dreamy Chorus", -3.0f },
{ "Funky Clean", -0.9f },
{ "Jazz Box", 7.2f },
{ "Country Snap", -3.9f },
{ "Surf 62", 0.9f },
{ "British Crunch", -0.5f },
{ "Classic Rock Marshall", -2.5f },
{ "Vintage Blues", -1.0f },
{ "Indie Rock Crunch", -1.8f },
{ "Tweed Breakup", -5.0f },
{ "Radio Rock", 2.3f },
{ "Stadium Rock", -0.5f },
{ "80s Lead", -2.4f },
{ "Heavy Rock", -0.3f },
{ "Hard Rock Lead", 1.2f },
{ "Brown Sound", -0.0f },
{ "Modern Alternative", 1.2f },
{ "Modern Metal", -0.2f },
{ "Thrash Mark", 1.0f },
{ "Nu Wall", 2.4f },
{ "Diesel Wall", 0.2f },
{ "Shoegaze Atmosphere", -2.9f },
{ "Ambient Guitar", 1.8f },
{ "Post-Rock", -0.4f },
{ "Dub Space", -2.0f },
{ "Vibe Machine", 3.1f },
{ "Britpop 95", -2.2f },
{ "Garage Fuzz", -7.1f },
{ "Octave Fuzz", 0.3f },
{ "Wah Rhythm", 5.9f },
{ "Desert Stoner", -0.2f },
{ "Seattle 92", 1.0f },
{ "Madchester", 2.1f },
{ "Whammy Dive", 0.6f },
    };

    const std::vector<Preset>& presetsFabrica()
    {
        static const std::vector<Preset> v = []
        {
            std::vector<Preset> r;
            for (const auto& d : defs)
            {
                std::string c = d.cadena;
                for (const auto& [n, db] : nivellsPreset)
                    if (std::string (n) == d.nom)
                    {
                        const auto barra = c.find ('|');
                        c.insert (barra == std::string::npos ? c.size() : barra, " sortida=" + std::to_string (db) + " ");
                    }
                r.push_back ({ d.nom, d.cat, d.desc, construeix (c) });
            }
            return r;
        }();
        return v;
    }

    const std::string& estatInicial()
    {
        static const std::string s = construeix (
            "amp=plexi canal=1 gain=5.5 bass=4.5 middle=7 treble=6 presence=5 cab=1960 alt=greenback mica=0 posa=0.3 dista=0.1 room=12 "
            "| ts808 off drive=2 tone=6 level=7 | dd3 loop off level=3 feedback=3 time=380 | rv6 loop level=3 time=4 mode=1");
        return s;
    }
}
