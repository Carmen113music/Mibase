#include "mibase/PresetsPads.h"

#include <algorithm>

namespace mibase::pads
{
    // Font: «50 seqüències d'acords — pop/rock, indie i funk» (llista d'en Roger, 05/11/2026).
    const std::vector<Preset>& presets()
    {
        static const std::vector<Preset> v {
            { "Creep", "Radiohead", "G", "Pop, rock i indie", 7, 0, { "G", "B", "C", "Cm", "G", "Em", "Am", "D" } },
            { "Where Is My Mind?", "Pixies", "E", "Pop, rock i indie", 4, 0, { "E", "C#m", "G#", "A", "E", "B", "C#m", "A" } },
            { "Boulevard of Broken Dreams", "Green Day", "F minor", "Pop, rock i indie", 5, 1, { "Fm", "Db", "Eb", "Bb", "Fm", "Ab", "Db", "C" } },
            { "Riptide", "Vance Joy", "A♭ major", "Pop, rock i indie", 8, 0, { "Bbm", "Ab", "Db", "Ab", "Bbm", "Fm", "Db", "Ab" } },
            { "Take Me Out", "Franz Ferdinand", "F minor", "Pop, rock i indie", 5, 1, { "Fm", "C", "Ab", "Eb", "Fm", "Db", "Ab", "C" } },
            { "Seven Nation Army", "The White Stripes", "E minor / E", "Pop, rock i indie", 4, 1, { "E", "G", "A", "B", "E", "D", "A", "E" } },
            { "Dani California", "Red Hot Chili Peppers", "A minor", "Pop, rock i indie", 9, 1, { "Am", "G", "F", "E", "Am", "C", "Dm", "E" } },
            { "Come Together", "The Beatles", "D minor", "Pop, rock i indie", 2, 1, { "Dm", "G", "C", "F", "Dm", "Am", "G", "A7" } },
            { "(I Can’t Get No) Satisfaction", "The Rolling Stones", "E major", "Pop, rock i indie", 4, 0, { "E", "A", "D", "A", "E", "B7", "A", "E" } },
            { "Paranoid Android", "Radiohead", "C major / A minor", "Pop, rock i indie", 0, 0, { "C", "D", "Em", "F", "G", "Am", "F", "E" } },
            { "Mr. Brightside", "The Killers", "D major / B minor", "Pop, rock i indie", 2, 0, { "D", "A", "Bm", "G", "D", "A", "G", "A" } },
            { "Why’d You Only Call Me When You’re High?", "Arctic Monkeys", "F# minor", "Pop, rock i indie", 6, 1, { "Dmaj7", "B7", "F#m7", "C#", "Dmaj7", "B7", "F#m7", "C#7" } },
            { "Lost Kitten", "Metric", "C# minor", "Pop, rock i indie", 1, 1, { "C#m", "F#m", "E", "Amaj7", "C#m", "G#m", "A", "B" } },
            { "Naive", "The Kooks", "B major", "Pop, rock i indie", 11, 0, { "Bmaj7", "E", "G#m", "F#", "B", "E", "C#m", "F#" } },
            { "Don’t Look Back in Anger", "Oasis", "C major", "Pop, rock i indie", 0, 0, { "C", "G", "Am", "E7", "F", "G", "C", "G" } },
            { "Champagne Supernova", "Oasis", "A major / F# minor", "Pop, rock i indie", 9, 0, { "A", "E", "D", "F#m", "A", "E", "D", "E" } },
            { "Snap Out of It", "Arctic Monkeys", "B♭ major / G minor", "Pop, rock i indie", 10, 0, { "Bb", "Fm", "Db", "C", "Bb", "Eb", "Gm", "F" } },
            { "I Love You", "Fontaines D.C.", "E minor", "Pop, rock i indie", 4, 1, { "Em", "G", "Am", "C", "Em", "D", "C", "B7" } },
            { "Arms", "The Paper Kites", "G major", "Pop, rock i indie", 7, 0, { "G", "D", "Bm", "A", "G", "Em", "C", "D" } },
            { "Big God", "Florence + The Machine", "F major / D minor", "Pop, rock i indie", 5, 0, { "Dm", "Bb", "C", "F", "Dm", "Gm", "Bb", "A7" } },
            { "Reckoner", "Radiohead", "E minor / G major", "Pop, rock i indie", 4, 1, { "Em", "Am", "C", "G", "Em", "D", "C", "B7" } },
            { "The Less I Know the Better", "Tame Impala", "C# minor", "Pop, rock i indie", 1, 1, { "C#m7", "F#m7", "G#m7", "C#m7", "Amaj7", "G#m7", "F#m7", "G#7" } },
            { "Feel Good Inc.", "Gorillaz", "E♭ minor", "Pop, rock i indie", 3, 1, { "Ebm", "Bbm", "Abm", "Db", "Ebm", "Gb", "Bbm", "Db" } },
            { "Dean Town", "Vulfpeck", "F# minor", "Funk", 6, 1, { "F#m7", "C#m7", "E7", "B7", "F#m7", "A7", "Dmaj7", "C#7" } },
            { "Pick Up the Pieces", "Average White Band", "F minor", "Funk", 5, 1, { "Fm7", "Cm7", "Bbm7", "Cm7", "Fm7", "Ab7", "Db7", "C7" } },
            { "Play That Funky Music", "Wild Cherry", "E minor / E funk", "Funk", 4, 1, { "Em7", "A7", "Em7", "A7", "G7", "A7", "Em7", "B7" } },
            { "Good Times", "Chic", "E minor", "Funk", 4, 1, { "Em7", "A7", "Em7", "A7", "Cmaj7", "B7", "Em7", "A7" } },
            { "Sex Machine", "James Brown", "E♭ dominant funk", "Funk", 3, 7, { "Eb9", "Eb9", "Ab9", "Ab9", "Eb9", "Eb9", "Bb9", "Ab9" } },
            { "Cissy Strut", "The Meters", "C mixolydian / C blues", "Funk", 0, 7, { "C7", "F7", "C7", "G7", "C7", "Bb7", "F7", "C7" } },
            { "Brick House", "The Commodores", "A minor", "Funk", 9, 1, { "Am7", "D7", "Am7", "D7", "G7", "C7", "Am7", "E7" } },
            { "Treasure", "Bruno Mars", "A♭ major", "Funk", 8, 0, { "Abmaj7", "Fm7", "Gm7", "Cm7", "Dbmaj7", "Bbm7", "Eb7", "Abmaj7" } },
            { "Superstition", "Stevie Wonder", "E♭ minor / blues", "Funk", 3, 1, { "Ebm7", "Abm7", "Ebm7", "Abm7", "B7", "Bb7", "Ebm7", "Bb7" } },
            { "Sing It Back", "Moloko", "B♭ minor", "Funk", 10, 1, { "Bbm7", "Ebm7", "Abm7", "Db7", "Gbmaj7", "F7", "Bbm7", "F7" } },
            { "Get Up (I Feel Like Being a) Sex Machine", "James Brown", "E dominant funk", "Funk", 4, 7, { "E9", "E9", "A9", "A9", "E9", "E9", "B9", "A9" } },
            { "Flashlight", "Parliament", "C minor", "Funk", 0, 1, { "Cm7", "Fm7", "Ab", "G7", "Cm7", "Bb", "Ab", "G7" } },
            { "Give Up the Funk", "Parliament", "C minor / funk", "Funk", 0, 1, { "Cm7", "F7", "Bb7", "Eb7", "Ab7", "G7", "Cm7", "G7" } },
            { "Thank You (Falettinme Be Mice Elf Agin)", "Sly & The Family Stone", "E♭ mixolydian / funk", "Funk", 3, 7, { "Eb7", "Ab7", "Eb7", "Bb7", "Ab7", "Eb7", "Db7", "Eb7" } },
            { "Jungle Boogie", "Kool & The Gang", "G minor funk", "Funk", 7, 1, { "Gm7", "Fm7", "Gm7", "Fm7", "Cm7", "D7", "Gm7", "D7" } },
            { "Uptown Funk", "Mark Ronson ft. Bruno Mars", "D minor funk", "Funk", 2, 1, { "Dm7", "G7", "Dm7", "G7", "Bbmaj7", "A7", "Dm7", "G7" } },
            { "Super Freak", "Rick James", "F minor", "Funk", 5, 1, { "Fm7", "Bbm7", "Eb7", "Abmaj7", "Dbmaj7", "C7", "Fm7", "C7" } },
            { "I Got You (I Feel Good)", "James Brown", "E dominant funk", "Funk", 4, 7, { "E7", "A7", "E7", "B7", "A7", "E7", "D7", "E7" } },
            { "Valerie", "Amy Winehouse", "E♭ major / C minor", "Pop, rock i indie", 3, 0, { "Eb", "Ab", "Cm", "Bb", "Eb", "Ab", "Fm", "Bb" } },
            { "Back to Black", "Amy Winehouse", "D minor", "Pop, rock i indie", 2, 1, { "Dm", "Bb", "F", "A7", "Dm", "Gm", "Bb", "A7" } },
            { "Rolling in the Deep", "Adele", "C minor", "Pop, rock i indie", 0, 1, { "Cm", "Eb", "Bb", "Ab", "Cm", "Eb", "Fm", "G7" } },
            { "Locked Out of Heaven", "Bruno Mars", "D minor / F major", "Pop, rock i indie", 2, 1, { "Dm", "F", "C", "Bb", "Dm", "F", "Gm", "A7" } },
            { "Royals", "Lorde", "D major / B minor", "Pop, rock i indie", 2, 0, { "D", "Bm", "G", "A", "D", "Bm", "Em", "A" } },
            { "Somebody That I Used to Know", "Gotye", "D minor / F major", "Pop, rock i indie", 2, 1, { "Dm", "Am", "C", "G", "Dm", "F", "C", "G" } },
            { "Electric Feel", "MGMT", "F# minor", "Pop, rock i indie", 6, 1, { "F#m", "D", "A", "E", "F#m", "C#m", "D", "E" } },
            { "Kids", "MGMT", "B minor", "Pop, rock i indie", 11, 1, { "Bm", "G", "D", "A", "Bm", "Em", "G", "A" } },
            { "Clocks", "Coldplay", "E♭ major / C minor", "Pop, rock i indie", 3, 0, { "Eb", "Bb", "Cm", "Ab", "Eb", "Bb", "Fm", "Ab" } },
        };
        return v;
    }

    namespace
    {
        constexpr int blanques[7] = { 0, 2, 4, 5, 7, 9, 11 };
        constexpr int negres[5] = { 1, 3, 6, 8, 10 };

        std::string acord (int to, const char* sufix) { return harmonia::nomTo (((to % 12) + 12) % 12) + sufix; }

        bool esMajor (const Tonalitat& t) { return harmonia::aEscala (t.to + 4, t); }

        /** Els 7 acords de la tonalitat (si l'escala en té menys, els de la major o menor). */
        std::vector<std::string> diatonics (const Tonalitat& t)
        {
            std::vector<std::string> r;
            for (auto& a : harmonia::acordsDeTonalitat (t, false)) r.push_back (a.nom);
            if (r.size() < 7)
            {
                r.clear();
                for (auto& a : harmonia::acordsDeTonalitat ({ t.to, esMajor (t) ? 0 : 1 }, false)) r.push_back (a.nom);
            }
            r.resize (7);
            return r;
        }

        std::vector<std::string> color (const Tonalitat& t)
        {
            const int to = t.to;
            if (esMajor (t))
                return { acord (to + 7, "7"), acord (to + 2, "m7"), acord (to + 5, "m"), acord (to + 8, ""), acord (to + 10, "") };
            return { acord (to + 7, "7"), acord (to, "m7"), acord (to + 5, ""), acord (to + 7, ""), acord (to + 1, "") };
        }

        /** Mateix acord (mateixes notes), escrit com sigui. */
        bool igual (const std::string& a, const std::string& b)
        {
            const auto x = harmonia::llegeixAcord (a), y = harmonia::llegeixAcord (b);
            return ! x.buit() && x == y;
        }
    }

    std::vector<std::string> padsDeTonalitat (const Tonalitat& t)
    {
        std::vector<std::string> r (numPads);
        const auto d = diatonics (t);
        const auto c = color (t);
        for (int i = 0; i < 7; ++i) r[(size_t) blanques[i]] = d[(size_t) i];
        for (int i = 0; i < 5; ++i) r[(size_t) negres[i]] = c[(size_t) i];
        return r;
    }

    std::vector<std::string> padsDePreset (int i)
    {
        // 0.12 (en Roger): els acords de la cançó per ordre, a partir de la C (C, C#, D…), sense repetir; la resta, buits.
        const auto& p = presets()[(size_t) std::clamp (i, 0, (int) presets().size() - 1)];
        std::vector<std::string> r;
        for (auto* a : p.acords)
        {
            bool hiEs = false;
            for (auto& u : r) if (igual (u, a)) hiEs = true;
            if (! hiEs && (int) r.size() < numPads) r.push_back (a);
        }
        r.resize (numPads);
        return r;
    }

    std::vector<std::string> adapta (const std::vector<std::string>& v, const std::string& preset)
    {
        if ((int) v.size() == numPads || v.empty()) return v;
        for (int i = 0; i < (int) presets().size(); ++i)
            if (preset == presets()[(size_t) i].titol) return padsDePreset (i);
        if (preset == "Acords de la tonalitat") return {};
        // Pads fets a mà: els 12 primers, en l'ordre de les tecles (blanques i després negres).
        std::vector<std::string> r (numPads);
        for (int k = 0; k < 12 && k < (int) v.size(); ++k)
            r[(size_t) (k < 7 ? blanques[k] : negres[k - 7])] = v[(size_t) k];
        return r;
    }
}
